#include "ext2.h"
#include "../drivers/ata.h"
#include "../serial.h"
#include "../mem/pmm.h"
#include "../limine.h"
#include "../cpu/critical.h"

extern volatile struct limine_hhdm_request hhdm_request;

#define EXT2_SUPER_MAGIC 0xEF53
#define EXT2_SUPERBLOCK_OFFSET 1024

// Uklad zgodny ze specyfikacja ext2 (https://www.nongnu.org/ext2-doc/ext2.html).
// Pola za "algo_bitmap" (preallocation, journal, htree...) sa dla nas na razie
// nieistotne i pominiete - Faza 1a odczytuje tylko to, co trzeba zeby zlokalizowac
// tablice deskryptorow grup blokow i zweryfikowac obraz.
struct __attribute__((packed)) Ext2Superblock {
    uint32_t inodes_count;
    uint32_t blocks_count;
    uint32_t r_blocks_count;
    uint32_t free_blocks_count;
    uint32_t free_inodes_count;
    uint32_t first_data_block;
    uint32_t log_block_size;
    uint32_t log_frag_size;
    uint32_t blocks_per_group;
    uint32_t frags_per_group;
    uint32_t inodes_per_group;
    uint32_t mtime;
    uint32_t wtime;
    uint16_t mnt_count;
    uint16_t max_mnt_count;
    uint16_t magic;
    uint16_t state;
    uint16_t errors;
    uint16_t minor_rev_level;
    uint32_t lastcheck;
    uint32_t checkinterval;
    uint32_t creator_os;
    uint32_t rev_level;
    uint16_t def_resuid;
    uint16_t def_resgid;
    // -- Ponizsze pola istnieja tylko gdy rev_level >= 1 (EXT2_DYNAMIC_REV), ale
    //    kazdy wspolczesny mke2fs go ustawia, wiec zakladamy ich obecnosc.
    uint32_t first_ino;
    uint16_t inode_size;
    uint16_t block_group_nr;
    uint32_t feature_compat;
    uint32_t feature_incompat;
    uint32_t feature_ro_compat;
    uint8_t  uuid[16];
    char     volume_name[16];
    char     last_mounted[64];
    uint32_t algo_bitmap;
};

struct __attribute__((packed)) Ext2GroupDesc {
    uint32_t block_bitmap;
    uint32_t inode_bitmap;
    uint32_t inode_table;
    uint16_t free_blocks_count;
    uint16_t free_inodes_count;
    uint16_t used_dirs_count;
    uint16_t pad;
    uint8_t  reserved[12];
};

static Ext2Superblock superblock;
static uint32_t block_size = 0;
static uint32_t block_groups_count = 0;
static Ext2GroupDesc* group_desc_table = nullptr;

static void print_uint32(uint32_t val) {
    char buf[12] = {0};
    int i = 0;
    if (val == 0) buf[i++] = '0';
    while (val > 0) { buf[i++] = '0' + (val % 10); val /= 10; }
    for (int j = i - 1; j >= 0; j--) SerialPort::WriteChar(buf[j]);
}

static void print_hex32(uint32_t val) {
    SerialPort::WriteString("0x");
    char buf[9] = {0};
    for (int i = 7; i >= 0; i--) {
        int nibble = (val >> (i * 4)) & 0xF;
        buf[7 - i] = (nibble < 10) ? ('0' + nibble) : ('a' + nibble - 10);
    }
    SerialPort::WriteString(buf);
}

// Faza 1a: sterownik ext2 nie jest jeszcze podlaczony do VFS/disk.img (ktory
// zostaje FAT32-owy - patrz PLAN_ext2_filesystem.md). Testujemy na osobnym
// obrazie mke2fs podlaczonym jako drugi dysk QEMU (-hdb == primary slave).
// W Fazie 3, po przelaczeniu disk.img na ext2, to wywolanie zmieni sie na
// ATA::ReadSectors (primary master, tak jak dzis FAT32).
static bool ReadDiskSector(uint32_t lba, uint8_t* buffer) {
    return ATA::ReadSectorSlave(lba, buffer);
}

static bool ReadDiskBytes(uint32_t byte_offset, uint32_t length, uint8_t* out) {
    uint32_t lba = byte_offset / 512;
    uint32_t sector_off = byte_offset % 512;
    uint32_t written = 0;
    uint8_t sector[512];

    while (written < length) {
        if (!ReadDiskSector(lba, sector)) return false;

        uint32_t copy_len = 512 - sector_off;
        if (copy_len > length - written) copy_len = length - written;

        for (uint32_t i = 0; i < copy_len; i++) out[written + i] = sector[sector_off + i];

        written += copy_len;
        sector_off = 0;
        lba++;
    }
    return true;
}

void Ext2::Init() {
    EnterCritical();

    if (!ReadDiskBytes(EXT2_SUPERBLOCK_OFFSET, sizeof(Ext2Superblock), (uint8_t*)&superblock)) {
        ExitCritical();
        SerialPort::WriteString("Ext2: Failed to read superblock.\n");
        return;
    }

    if (superblock.magic != EXT2_SUPER_MAGIC) {
        ExitCritical();
        SerialPort::WriteString("Ext2: Invalid magic number - not an ext2 filesystem.\n");
        return;
    }

    if (superblock.blocks_per_group == 0 || superblock.inodes_per_group == 0) {
        ExitCritical();
        SerialPort::WriteString("Ext2: Invalid superblock (zero blocks/inodes per group).\n");
        return;
    }

    block_size = 1024u << superblock.log_block_size;
    block_groups_count = (superblock.blocks_count + superblock.blocks_per_group - 1) / superblock.blocks_per_group;

    // Tablica deskryptorow grup blokow zaczyna sie w bloku zaraz za superblokiem:
    // trzeci blok (0,1,2) przy 1KiB blokach (first_data_block == 1), drugi (0,1)
    // przy wiekszych blokach (first_data_block == 0).
    uint32_t bgdt_block = superblock.first_data_block + 1;
    uint32_t bgdt_bytes = block_groups_count * sizeof(Ext2GroupDesc);
    uint32_t bgdt_pages = (bgdt_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    if (bgdt_pages == 0) bgdt_pages = 1;

    void* phys_ptr = PMM::AllocatePages(bgdt_pages);
    if (!phys_ptr) {
        ExitCritical();
        SerialPort::WriteString("Ext2: Failed to allocate memory for group descriptor table.\n");
        return;
    }
    group_desc_table = (Ext2GroupDesc*)((uint64_t)phys_ptr + hhdm_request.response->offset);

    if (!ReadDiskBytes((uint64_t)bgdt_block * block_size, bgdt_bytes, (uint8_t*)group_desc_table)) {
        ExitCritical();
        SerialPort::WriteString("Ext2: Failed to read block group descriptor table.\n");
        return;
    }

    ExitCritical();

    SerialPort::WriteString("Ext2: Initialized successfully.\n");

    // Diagnostyka do reczne porownania z `dumpe2fs test.img` na hoscie (weryfikacja Fazy 1a).
    SerialPort::WriteString("Ext2: magic="); print_hex32(superblock.magic);
    SerialPort::WriteString(" block_size="); print_uint32(block_size);
    SerialPort::WriteString(" blocks_count="); print_uint32(superblock.blocks_count);
    SerialPort::WriteString(" inodes_count="); print_uint32(superblock.inodes_count);
    SerialPort::WriteString("\n");
    SerialPort::WriteString("Ext2: blocks_per_group="); print_uint32(superblock.blocks_per_group);
    SerialPort::WriteString(" inodes_per_group="); print_uint32(superblock.inodes_per_group);
    SerialPort::WriteString(" first_data_block="); print_uint32(superblock.first_data_block);
    SerialPort::WriteString(" block_groups_count="); print_uint32(block_groups_count);
    SerialPort::WriteString("\n");

    SerialPort::WriteString("Ext2: group[0] block_bitmap="); print_uint32(group_desc_table[0].block_bitmap);
    SerialPort::WriteString(" inode_bitmap="); print_uint32(group_desc_table[0].inode_bitmap);
    SerialPort::WriteString(" inode_table="); print_uint32(group_desc_table[0].inode_table);
    SerialPort::WriteString(" free_blocks="); print_uint32(group_desc_table[0].free_blocks_count);
    SerialPort::WriteString(" free_inodes="); print_uint32(group_desc_table[0].free_inodes_count);
    SerialPort::WriteString("\n");
}
