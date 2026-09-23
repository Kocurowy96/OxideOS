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

// Uklad i-wezla wg specyfikacji ext2 (rewizja 0/1, 128 bajtow) - Faza 1b. osd1/osd2
// (pola zalezne od OS tworzacego system plikow) sa nam na razie niepotrzebne, ale
// trzymane w strukturze zeby zachowac poprawny total size/offsety kolejnych pol.
struct __attribute__((packed)) Ext2Inode {
    uint16_t mode;
    uint16_t uid;
    uint32_t size;          // dolne 32 bity - dla plikow >4GB trzeba tez dir_acl (size_high)
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint16_t gid;
    uint16_t links_count;
    uint32_t blocks;        // liczba 512-bajtowych sektorow (NIE blokow block_size!)
    uint32_t flags;
    uint32_t osd1;
    uint32_t block[15];     // 12 bezposrednich + pojedynczo/podwojnie/potrojnie posredni
    uint32_t generation;
    uint32_t file_acl;
    uint32_t dir_acl;       // size_high dla plikow >4GB (na razie nieobslugiwane)
    uint32_t faddr;
    uint8_t  osd2[12];
};

#define EXT2_S_IFMT  0xF000
#define EXT2_S_IFDIR 0x4000
#define EXT2_S_IFREG 0x8000
#define EXT2_ROOT_INODE 2

// Faza 1c wspiera bloki do 4KiB (najwiekszy rozmiar jaki wspolczesny mke2fs realnie
// uzywa na x86 - domyslny to 4096). Wieksze bloki (rzadkie, wymagalyby stron > 4KiB)
// sa odrzucane w ReadDirectoryFirstBlock zamiast przepelnic ponizszy bufor.
#define EXT2_MAX_BLOCK_SIZE 4096

// Format wpisu katalogowego ext2 (klasyczny, bez htree): stala czesc + nazwa BEZ
// terminatora '\0' o dlugosci name_len zaraz po strukturze. Kolejny wpis zaczyna sie
// rec_len bajtow dalej - wpisy wypelniaja caly blok, ostatni w bloku ma rec_len
// "dociagniety" do konca bloku (nie ma jawnego terminatora listy wpisow).
struct __attribute__((packed)) Ext2DirEntry {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t  name_len;
    uint8_t  file_type;
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

// Faza 1b: lokalizacja i-wezla po numerze (numeracja od 1, i-wezel 0 nie istnieje).
// grupa = (inode_nr - 1) / inodes_per_group, indeks w grupie = (inode_nr - 1) %
// inodes_per_group, offset bajtowy = group_desc_table[grupa].inode_table * block_size
// + indeks * inode_size (inode_size z superbloku - juz wczytywany w Init(), dostepny
// tu jako pole statycznego modulowego `superblock`, wiec nie trzeba osobnej zmiennej).
static bool ReadInode(uint32_t inode_nr, Ext2Inode* out) {
    if (inode_nr == 0 || group_desc_table == nullptr) return false;

    uint32_t index = inode_nr - 1;
    uint32_t group = index / superblock.inodes_per_group;
    uint32_t index_in_group = index % superblock.inodes_per_group;
    if (group >= block_groups_count) return false;

    uint32_t inode_size = superblock.inode_size ? superblock.inode_size : 128;
    uint64_t byte_offset = (uint64_t)group_desc_table[group].inode_table * block_size
                          + (uint64_t)index_in_group * inode_size;

    // inode_size moze byc wiekszy niz sizeof(Ext2Inode) (np. 256 przy nowszych
    // mke2fs z rozszerzonymi atrybutami) - czytamy tylko pola ktore rozumiemy.
    uint32_t read_size = sizeof(Ext2Inode);
    if (inode_size < read_size) read_size = inode_size;

    return ReadDiskBytes((uint32_t)byte_offset, read_size, (uint8_t*)out);
}

// Faza 1c: czyta tylko pierwszy blok bezposredni katalogu (block[0]). Uproszczenie
// swiadome - katalogi wieksze niz jeden blok (np. >200 wpisow przy 1KiB blokach) nie
// sa jeszcze obslugiwane, do rozszerzenia przy okazji 1e/1f (odczyt przez kolejne
// bloki bezposrednie/posrednie tak jak dla zwyklych plikow).
static bool ReadDirectoryFirstBlock(const Ext2Inode* dir_inode, uint8_t* out_buffer) {
    if (block_size == 0 || block_size > EXT2_MAX_BLOCK_SIZE) return false;
    if (dir_inode->block[0] == 0) return false;

    uint64_t byte_offset = (uint64_t)dir_inode->block[0] * block_size;
    return ReadDiskBytes((uint32_t)byte_offset, block_size, out_buffer);
}

// Faza 1c: przeszukuje bufor z zawartoscia bloku katalogu (patrz ReadDirectoryFirstBlock)
// w poszukiwaniu wpisu o podanej nazwie. `name` NIE jest zakonczone '\0' - porownujemy
// dokladnie `name_len` bajtow, tak jak nazwy wpisow w samym obrazie.
static bool FindEntryInBlock(const uint8_t* block_buf, uint32_t buf_len, const char* name, uint32_t name_len, uint32_t* out_inode) {
    uint32_t offset = 0;
    while (offset + sizeof(Ext2DirEntry) <= buf_len) {
        const Ext2DirEntry* entry = (const Ext2DirEntry*)(block_buf + offset);
        if (entry->rec_len < sizeof(Ext2DirEntry)) break; // uszkodzony/pusty blok - nie ma jak kontynuowac

        if (entry->inode != 0 && entry->name_len == name_len) {
            const char* entry_name = (const char*)(block_buf + offset + sizeof(Ext2DirEntry));
            bool match = true;
            for (uint32_t i = 0; i < name_len; i++) {
                if (entry_name[i] != name[i]) { match = false; break; }
            }
            if (match) {
                *out_inode = entry->inode;
                return true;
            }
        }

        offset += entry->rec_len;
    }
    return false;
}

// Faza 1c: rozwiazuje sciezke (np. "/usr/bin/HELLO.ELF") na numer i-wezla, zaczynajac
// od i-wezla root (2). Kazdy czlon sciezki (oprocz ostatniego) musi byc katalogiem
// zeby kontynuowac - sprawdzane przez odczyt jego i-wezla na poczatku kolejnej
// iteracji. Analogiczne do FindDirectoryCluster/ResolveDirectoryCluster w fat32.cpp,
// tylko ze operuje na i-wezlach zamiast klastrach FAT. Jeszcze NIE jest publicznym API
// Ext2:: (prywatna, jak ReadInode) - skorzysta z niej dopiero Faza 1d (ListDirectory)
// i 1e/1f (ReadFile).
static bool ResolvePath(const char* path, uint32_t* out_inode_nr) {
    if (path[0] == '\0' || (path[0] == '/' && path[1] == '\0')) {
        *out_inode_nr = EXT2_ROOT_INODE;
        return true;
    }

    uint32_t current_inode_nr = EXT2_ROOT_INODE;
    int path_idx = (path[0] == '/') ? 1 : 0;

    // Uwaga na przyszlosc (Faza 1d/1e+): ten bufor zyje na stosie wywolujacego. Dopoki
    // ResolvePath jest wolane tylko z Init() (rozruch, duzy stos) jest to bezpieczne;
    // gdyby w kolejnych fazach trafilo do wywolan syscalli na 8KiB stosie jadra per-task
    // (patrz kernel/proc/sched.cpp), warto to zrewidowac (np. bufor statyczny pod locka).
    uint8_t block_buf[EXT2_MAX_BLOCK_SIZE];

    while (path[path_idx] != '\0') {
        char segment[256];
        uint32_t seg_len = 0;
        while (path[path_idx] != '\0' && path[path_idx] != '/' && seg_len < sizeof(segment) - 1) {
            segment[seg_len++] = path[path_idx++];
        }
        while (path[path_idx] == '/') path_idx++; // pochlania kolejne '/' (np. podwojny separator)

        if (seg_len == 0) continue; // np. koncowy '/'

        Ext2Inode dir_inode;
        if (!ReadInode(current_inode_nr, &dir_inode)) return false;
        if ((dir_inode.mode & EXT2_S_IFMT) != EXT2_S_IFDIR) return false;

        if (!ReadDirectoryFirstBlock(&dir_inode, block_buf)) return false;

        uint32_t found_inode = 0;
        if (!FindEntryInBlock(block_buf, block_size, segment, seg_len, &found_inode)) return false;

        current_inode_nr = found_inode;
    }

    *out_inode_nr = current_inode_nr;
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

    // Faza 1b: odczyt i-wezla root (zawsze numer 2 w ext2) - do reczne porownania
    // z `debugfs -R "stat <2>" test.img` na hoscie (weryfikacja Fazy 1b).
    Ext2Inode root_inode;
    if (!ReadInode(EXT2_ROOT_INODE, &root_inode)) {
        SerialPort::WriteString("Ext2: Failed to read root inode.\n");
        return;
    }

    SerialPort::WriteString("Ext2: root inode mode="); print_hex32(root_inode.mode);
    if ((root_inode.mode & EXT2_S_IFMT) == EXT2_S_IFDIR) {
        SerialPort::WriteString(" (S_IFDIR - OK)");
    } else {
        SerialPort::WriteString(" (NOT a directory - unexpected!)");
    }
    SerialPort::WriteString(" size="); print_uint32(root_inode.size);
    SerialPort::WriteString(" links_count="); print_uint32(root_inode.links_count);
    SerialPort::WriteString(" blocks="); print_uint32(root_inode.blocks);
    SerialPort::WriteString("\n");
    SerialPort::WriteString("Ext2: root inode block[0]="); print_uint32(root_inode.block[0]);
    SerialPort::WriteString(" block[1]="); print_uint32(root_inode.block[1]);
    SerialPort::WriteString("\n");

    // Faza 1c: test ResolvePath na "/lost+found" - katalog ktory kazdy swiezy obraz
    // mke2fs tworzy domyslnie w roocie, wiec (w przeciwienstwie do dowolnej nazwy pliku
    // testowego wgranego reczne przez debugfs) ten test dziala na kazdym obrazie bez
    // dodatkowego przygotowania. Do reczne porownania z
    // `debugfs -R "stat <lost+found>" test.img` (ten sam numer i-wezla).
    uint32_t lost_found_inode = 0;
    if (ResolvePath("/lost+found", &lost_found_inode)) {
        SerialPort::WriteString("Ext2: ResolvePath(/lost+found) -> inode=");
        print_uint32(lost_found_inode);
        SerialPort::WriteString("\n");
    } else {
        SerialPort::WriteString("Ext2: ResolvePath(/lost+found) failed.\n");
    }
}
