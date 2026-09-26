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

// Faza 2c: wartosci pola file_type wpisu katalogowego (feature "filetype", wlaczona
// domyslnie przez wspolczesny mke2fs - patrz superblock.feature_incompat, widoczne
// tez w dumpe2fs jako "filetype" na liscie Filesystem features). Musimy je ustawiac
// poprawnie przy tworzeniu nowego wpisu, inaczej e2fsck zglosi niezgodnosc z i_mode.
#define EXT2_FT_REG_FILE 1
#define EXT2_FT_DIR 2

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
static uint32_t bgdt_block = 0; // Faza 2a: zapamietane z Init(), potrzebne zeby zapisac
                                // pojedynczy Ext2GroupDesc z powrotem po alokacji/zwolnieniu.

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

// Faza 2a: analogicznie do ReadDiskSector - w Fazie 3b zamieni sie na ATA::WriteSectors
// (primary master), na razie pisze na osobny obraz testowy podpiety jako -hdb.
static bool WriteDiskSector(uint32_t lba, const uint8_t* buffer) {
    return ATA::WriteSectorSlave(lba, buffer);
}

// Odpowiednik ReadDiskBytes dla zapisu. Pelny sektor pisany jest wprost z `data`;
// czesciowy (pierwszy/ostatni sektor zakresu, jesli byte_offset/length nie sa
// wyrownane do 512) wymaga odczytu-modyfikacji-zapisu, zeby nie nadpisac bajtow
// sasiadujacych w tym samym sektorze spoza zakresu `[byte_offset, byte_offset+length)`.
static bool WriteDiskBytes(uint32_t byte_offset, uint32_t length, const uint8_t* data) {
    uint32_t lba = byte_offset / 512;
    uint32_t sector_off = byte_offset % 512;
    uint32_t written = 0;
    uint8_t sector[512];

    while (written < length) {
        uint32_t copy_len = 512 - sector_off;
        if (copy_len > length - written) copy_len = length - written;

        if (copy_len == 512) {
            if (!WriteDiskSector(lba, data + written)) return false;
        } else {
            if (!ReadDiskSector(lba, sector)) return false;
            for (uint32_t i = 0; i < copy_len; i++) sector[sector_off + i] = data[written + i];
            if (!WriteDiskSector(lba, sector)) return false;
        }

        written += copy_len;
        sector_off = 0;
        lba++;
    }
    return true;
}

// Faza 2a: zapisuje z powrotem jeden wpis tablicy deskryptorow grup (po zmianie
// free_blocks_count przy alokacji/zwolnieniu bloku) pod jego pozycja w BGDT na dysku.
// Uwaga (swiadome uproszczenie, zgodnie z PLAN_ext2_filesystem.md - brak obslugi
// kopii zapasowych): aktualizujemy tylko podstawowa kopie BGDT/superbloku (grupa 0
// wzgledem bloku first_data_block+1). Prawdziwy ext2 trzyma kopie zapasowe w
// niektorych grupach (sparse_super), ale wszystkie obrazy testowe i produkcyjny
// disk.img (64MB, jeden blok_group przy domyslnym mke2fs) maja dzis dokladnie jedna
// grupe, wiec kopii zapasowych po prostu nie ma - nie ma nic do synchronizacji.
static bool WriteBackGroupDesc(uint32_t group) {
    uint64_t offset = (uint64_t)bgdt_block * block_size + (uint64_t)group * sizeof(Ext2GroupDesc);
    return WriteDiskBytes((uint32_t)offset, sizeof(Ext2GroupDesc), (const uint8_t*)&group_desc_table[group]);
}

static bool WriteBackSuperblock() {
    return WriteDiskBytes(EXT2_SUPERBLOCK_OFFSET, sizeof(Ext2Superblock), (const uint8_t*)&superblock);
}

// Faza 2a: bitmapa wolnych blokow danych, bit-per-blok (1 = zajety), jedna na grupe
// (group_desc_table[grupa].block_bitmap wskazuje numer bloku bitmapy). Szuka pierwszego
// zerowego bitu przechodzac po grupach w kolejnosci (pomijajac grupy z free_blocks_count
// == 0 - czysta optymalizacja, nie trzeba wtedy w ogole czytac ich bitmapy), ustawia go
// na 1, aktualizuje liczniki free_blocks_count w grupie i w superbloku, zapisuje obie
// zmiany na dysk (WriteBackGroupDesc/WriteBackSuperblock) razem z sama bitmapa - jesli
// ktorykolwiek z tych zapisow zawiedzie, zwraca false, ale bitmapa i liczniki w pamieci
// (group_desc_table/superblock) zostaja juz zmienione, wiec kolejna alokacja nie
// przydzieli tego samego bloku ponownie (bezpieczne w gorsza strone - zgubiony wolny
// blok, nie podwojna alokacja).
static bool AllocateBlock(uint32_t* out_block_num) {
    if (block_size == 0 || block_size > EXT2_MAX_BLOCK_SIZE || group_desc_table == nullptr) return false;

    EnterCritical();

    uint8_t bitmap[EXT2_MAX_BLOCK_SIZE];
    for (uint32_t group = 0; group < block_groups_count; group++) {
        if (group_desc_table[group].free_blocks_count == 0) continue;

        uint32_t group_start_block = superblock.first_data_block + group * superblock.blocks_per_group;
        uint32_t blocks_in_group = superblock.blocks_per_group;
        if (group == block_groups_count - 1) {
            uint32_t remaining = superblock.blocks_count - group_start_block;
            if (remaining < blocks_in_group) blocks_in_group = remaining;
        }

        uint64_t bitmap_offset = (uint64_t)group_desc_table[group].block_bitmap * block_size;
        if (!ReadDiskBytes((uint32_t)bitmap_offset, block_size, bitmap)) {
            ExitCritical();
            return false;
        }

        for (uint32_t bit = 0; bit < blocks_in_group; bit++) {
            uint32_t byte_idx = bit / 8;
            uint8_t mask = (uint8_t)(1 << (bit % 8));
            if (bitmap[byte_idx] & mask) continue; // zajety

            bitmap[byte_idx] |= mask;
            if (!WriteDiskBytes((uint32_t)bitmap_offset, block_size, bitmap)) {
                ExitCritical();
                return false;
            }

            group_desc_table[group].free_blocks_count--;
            superblock.free_blocks_count--;
            bool ok = WriteBackGroupDesc(group) && WriteBackSuperblock();
            ExitCritical();
            if (!ok) return false;

            *out_block_num = group_start_block + bit;
            return true;
        }
    }

    ExitCritical();
    SerialPort::WriteString("Ext2: AllocateBlock - no free blocks.\n");
    return false;
}

// Faza 2a: odwrotnosc AllocateBlock. block_num musi byc >= first_data_block (blok 0
// przed first_data_block przy 1KiB blokach nie jest czescia zadnej grupy). Wywolanie
// na juz-wolnym bloku (podwojne zwolnienie - blad wywolujacego) jest wykrywane i
// odrzucane, zeby nie zawyzyc free_blocks_count.
static bool FreeBlock(uint32_t block_num) {
    if (block_size == 0 || block_size > EXT2_MAX_BLOCK_SIZE || group_desc_table == nullptr) return false;
    if (block_num < superblock.first_data_block) return false;

    uint32_t rel = block_num - superblock.first_data_block;
    uint32_t group = rel / superblock.blocks_per_group;
    uint32_t bit = rel % superblock.blocks_per_group;
    if (group >= block_groups_count) return false;

    EnterCritical();

    uint8_t bitmap[EXT2_MAX_BLOCK_SIZE];
    uint64_t bitmap_offset = (uint64_t)group_desc_table[group].block_bitmap * block_size;
    if (!ReadDiskBytes((uint32_t)bitmap_offset, block_size, bitmap)) {
        ExitCritical();
        return false;
    }

    uint32_t byte_idx = bit / 8;
    uint8_t mask = (uint8_t)(1 << (bit % 8));
    if (!(bitmap[byte_idx] & mask)) {
        ExitCritical();
        SerialPort::WriteString("Ext2: FreeBlock - double free (block already marked free).\n");
        return false;
    }

    bitmap[byte_idx] &= (uint8_t)~mask;
    if (!WriteDiskBytes((uint32_t)bitmap_offset, block_size, bitmap)) {
        ExitCritical();
        return false;
    }

    group_desc_table[group].free_blocks_count++;
    superblock.free_blocks_count++;
    bool ok = WriteBackGroupDesc(group) && WriteBackSuperblock();
    ExitCritical();
    return ok;
}

// Faza 2b: bitmapa wolnych i-wezlow - 1:1 ten sam wzor co AllocateBlock/FreeBlock
// wyzej, tylko na group_desc_table[].inode_bitmap/free_inodes_count zamiast
// block_bitmap/free_blocks_count. Kluczowa roznica: numeracja i-wezlow zaczyna sie
// od 1 (nie ma odpowiednika first_data_block przy blokach), wiec bit `i` w bitmapie
// grupy `g` odpowiada i-wezlowi `g*inodes_per_group + i + 1`. I-wezly 1-10
// (zarezerwowane specyfikacja ext2 - m.in. 2 = root) maja bity juz ustawione przez
// mke2fs, wiec zwykle liniowe przeszukiwanie od bitu 0 samo je pomija bez
// specjalnego przypadku. Nie zeruje/nie dotyka tresci samego i-wezla na dysku
// (mode/size/block[]...) - to zadanie tego kto woła AllocateInode (Faza 2c), tu
// zajmujemy sie wylacznie bitmapa i licznikami.
static bool AllocateInode(uint32_t* out_inode_nr) {
    if (block_size == 0 || block_size > EXT2_MAX_BLOCK_SIZE || group_desc_table == nullptr) return false;

    EnterCritical();

    uint8_t bitmap[EXT2_MAX_BLOCK_SIZE];
    for (uint32_t group = 0; group < block_groups_count; group++) {
        if (group_desc_table[group].free_inodes_count == 0) continue;

        uint32_t inodes_in_group = superblock.inodes_per_group;
        if (group == block_groups_count - 1) {
            uint32_t accounted = group * superblock.inodes_per_group;
            uint32_t remaining = superblock.inodes_count - accounted;
            if (remaining < inodes_in_group) inodes_in_group = remaining;
        }

        uint64_t bitmap_offset = (uint64_t)group_desc_table[group].inode_bitmap * block_size;
        if (!ReadDiskBytes((uint32_t)bitmap_offset, block_size, bitmap)) {
            ExitCritical();
            return false;
        }

        for (uint32_t bit = 0; bit < inodes_in_group; bit++) {
            uint32_t byte_idx = bit / 8;
            uint8_t mask = (uint8_t)(1 << (bit % 8));
            if (bitmap[byte_idx] & mask) continue; // zajety

            bitmap[byte_idx] |= mask;
            if (!WriteDiskBytes((uint32_t)bitmap_offset, block_size, bitmap)) {
                ExitCritical();
                return false;
            }

            group_desc_table[group].free_inodes_count--;
            superblock.free_inodes_count--;
            bool ok = WriteBackGroupDesc(group) && WriteBackSuperblock();
            ExitCritical();
            if (!ok) return false;

            *out_inode_nr = group * superblock.inodes_per_group + bit + 1;
            return true;
        }
    }

    ExitCritical();
    SerialPort::WriteString("Ext2: AllocateInode - no free inodes.\n");
    return false;
}

// Faza 2b: odwrotnosc AllocateInode, analogiczna do FreeBlock.
static bool FreeInode(uint32_t inode_nr) {
    if (block_size == 0 || block_size > EXT2_MAX_BLOCK_SIZE || group_desc_table == nullptr) return false;
    if (inode_nr == 0) return false;

    uint32_t index = inode_nr - 1;
    uint32_t group = index / superblock.inodes_per_group;
    uint32_t bit = index % superblock.inodes_per_group;
    if (group >= block_groups_count) return false;

    EnterCritical();

    uint8_t bitmap[EXT2_MAX_BLOCK_SIZE];
    uint64_t bitmap_offset = (uint64_t)group_desc_table[group].inode_bitmap * block_size;
    if (!ReadDiskBytes((uint32_t)bitmap_offset, block_size, bitmap)) {
        ExitCritical();
        return false;
    }

    uint32_t byte_idx = bit / 8;
    uint8_t mask = (uint8_t)(1 << (bit % 8));
    if (!(bitmap[byte_idx] & mask)) {
        ExitCritical();
        SerialPort::WriteString("Ext2: FreeInode - double free (inode already marked free).\n");
        return false;
    }

    bitmap[byte_idx] &= (uint8_t)~mask;
    if (!WriteDiskBytes((uint32_t)bitmap_offset, block_size, bitmap)) {
        ExitCritical();
        return false;
    }

    group_desc_table[group].free_inodes_count++;
    superblock.free_inodes_count++;
    bool ok = WriteBackGroupDesc(group) && WriteBackSuperblock();
    ExitCritical();
    return ok;
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

// Faza 2c: odwrotnosc ReadInode - zapisuje tylko pola ktore rozumiemy (sizeof(Ext2Inode)
// = 128 bajtow, rev0/1). Jesli inode_size na tym obrazie jest wiekszy (np. 256 -
// powszechny domyslny u wspolczesnego mke2fs nawet dla ext2), bajty od 128 wzwyz
// (rozszerzone atrybuty/i_extra_isize, ktorych i tak nie uzywamy) zostaja nietkniete -
// dla swiezo zaalokowanego i-wezla powinny juz byc wyzerowane przez mke2fs przy
// formatowaniu, wiec nie ma czego synchronizowac (to samo swiadome uproszczenie co
// przy pominietych kopiach zapasowych BGDT w Fazie 2a).
static bool WriteInode(uint32_t inode_nr, const Ext2Inode* in) {
    if (inode_nr == 0 || group_desc_table == nullptr) return false;

    uint32_t index = inode_nr - 1;
    uint32_t group = index / superblock.inodes_per_group;
    uint32_t index_in_group = index % superblock.inodes_per_group;
    if (group >= block_groups_count) return false;

    uint32_t inode_size = superblock.inode_size ? superblock.inode_size : 128;
    uint64_t byte_offset = (uint64_t)group_desc_table[group].inode_table * block_size
                          + (uint64_t)index_in_group * inode_size;

    uint32_t write_size = sizeof(Ext2Inode);
    if (inode_size < write_size) write_size = inode_size;

    return WriteDiskBytes((uint32_t)byte_offset, write_size, (const uint8_t*)in);
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

// Faza 2c: odwrotnosc ReadDirectoryFirstBlock - zapisuje zawartosc bloku z powrotem
// po wstawieniu nowego wpisu katalogowego (patrz FindDirInsertSlot/Ext2::WriteFile).
// Ta sama uproszczona zalozenie "tylko block[0]" - wstawianie wpisu do katalogu
// wiekszego niz jeden blok nie jest jeszcze obslugiwane.
static bool WriteDirectoryFirstBlock(const Ext2Inode* dir_inode, const uint8_t* in_buffer) {
    if (block_size == 0 || block_size > EXT2_MAX_BLOCK_SIZE) return false;
    if (dir_inode->block[0] == 0) return false;

    uint64_t byte_offset = (uint64_t)dir_inode->block[0] * block_size;
    return WriteDiskBytes((uint32_t)byte_offset, block_size, in_buffer);
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

// Faza 2c: szuka miejsca na nowy wpis katalogowy o dlugosci `needed_len` (juz
// zaokraglonej do 4 bajtow, naglowek+nazwa) w buforze bloku katalogu. Dwa przypadki,
// oba standardowe dla klasycznego (bez htree) formatu katalogow ext2:
//  1. Pusty/skasowany wpis (entry->inode == 0) z rec_len >= needed_len - uzywamy go
//     w calosci, zachowujac jego dotychczasowy rec_len (nie przycinamy).
//  2. Zywy wpis, ktorego rec_len jest wiekszy niz jego "prawdziwa" dlugosc (naglowek+
//     jego wlasna nazwa, zaokraglona do 4B) o co najmniej needed_len - dzielimy go:
//     przycinamy jego rec_len do prawdziwej dlugosci, a nowy wpis dostaje reszte.
//     To najczestszy przypadek w praktyce - ostatni wpis w bloku ma zwykle rec_len
//     "dociagniety" az do konca bloku (patrz komentarz przy Ext2DirEntry wyzej), wiec
//     zazwyczaj to on ma cala nadwyzke miejsca.
// Mutuje `block_buf` w miejscu (przycina rec_len dzielonego wpisu) - wywolujacy sam
// dopisuje tresc nowego wpisu pod zwroconym `out_offset`/`out_rec_len` i zapisuje caly
// bufor z powrotem (WriteDirectoryFirstBlock). Zwraca false gdy w bloku brakuje
// miejsca (Faza 2c: tylko pierwszy blok katalogu jest przeszukiwany/rozszerzany -
// dodanie kolejnego bloku do katalogu, gdy pierwszy jest calkiem pelny, nie jest
// jeszcze obslugiwane, tak jak ograniczenie ReadDirectoryFirstBlock/ResolvePath).
static bool FindDirInsertSlot(uint8_t* block_buf, uint32_t buf_len, uint32_t needed_len, uint32_t* out_offset, uint16_t* out_rec_len) {
    uint32_t offset = 0;
    while (offset + sizeof(Ext2DirEntry) <= buf_len) {
        Ext2DirEntry* entry = (Ext2DirEntry*)(block_buf + offset);
        if (entry->rec_len < sizeof(Ext2DirEntry)) break; // uszkodzony/pusty blok

        if (entry->inode == 0) {
            if (entry->rec_len >= needed_len) {
                *out_offset = offset;
                *out_rec_len = entry->rec_len;
                return true;
            }
        } else {
            uint32_t actual_len = (sizeof(Ext2DirEntry) + entry->name_len + 3) & ~3u;
            if (actual_len <= entry->rec_len && (entry->rec_len - actual_len) >= needed_len) {
                uint16_t leftover = (uint16_t)(entry->rec_len - actual_len);
                entry->rec_len = (uint16_t)actual_len; // przycinamy dzielony wpis
                *out_offset = offset + actual_len;
                *out_rec_len = leftover;
                return true;
            }
        }

        offset += entry->rec_len;
    }
    return false;
}

// Faza 2c: rozdziela sciezke na katalog-rodzic i nazwe ostatniego czlonu, np.
// "/usr/bin/HELLO.ELF" -> parent_path="/usr/bin", name="HELLO.ELF". Wymaga sciezki
// zaczynajacej sie od '/' (tak jak wszystkie sciezki w OxideOS - VFS:: zawsze
// przekazuje je z wiodacym '/', patrz vfs.cpp) - `path == "/"` samo (bez pliku) nie
// jest poprawnym argumentem dla WriteFile i jest odrzucane.
static bool SplitParentAndName(const char* path, char* out_parent_path, char* out_name, uint32_t* out_name_len) {
    if (path[0] != '/') return false;

    int len = 0;
    while (path[len] != '\0') len++;

    int last_slash = -1;
    for (int i = 0; i < len; i++) if (path[i] == '/') last_slash = i;
    if (last_slash < 0) return false; // niemozliwe skoro path[0] == '/', ale dla bezpieczenstwa

    int name_start = last_slash + 1;
    uint32_t name_len = (uint32_t)(len - name_start);
    if (name_len == 0 || name_len > 255) return false; // koncowy '/' albo za dluga nazwa

    for (uint32_t i = 0; i < name_len; i++) out_name[i] = path[name_start + i];
    out_name[name_len] = '\0';
    *out_name_len = name_len;

    if (last_slash == 0) {
        out_parent_path[0] = '/';
        out_parent_path[1] = '\0';
    } else {
        for (int i = 0; i < last_slash; i++) out_parent_path[i] = path[i];
        out_parent_path[last_slash] = '\0';
    }
    return true;
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

// Faza 1d: pierwsza publiczna funkcja Ext2:: z prawdziwa implementacja poza Init().
// Rozwiazuje `path` przez ResolvePath (Faza 1c), sprawdza ze to katalog, czyta jego
// pierwszy blok (ReadDirectoryFirstBlock - to samo uproszczenie "tylko block[0]" co w
// ResolvePath) i wypelnia out_entries pomijajac puste wpisy (inode == 0) oraz "."/".."
// (analogicznie do FAT32::ListDirectory ktore pomija wpisy zaczynajace sie od '.',
// patrz fat32.cpp:639). Wpis katalogowy ext2 ma tylko inode+file_type, wiec size i
// attributes wymagaja dodatkowego ReadInode na kazdym znalezionym wpisie.
int Ext2::ListDirectory(const char* path, DirEntry* out_entries, int max_entries) {
    uint32_t dir_inode_nr = 0;
    if (!ResolvePath(path, &dir_inode_nr)) {
        SerialPort::WriteString("Ext2: ListDirectory - path not found.\n");
        return 0;
    }

    Ext2Inode dir_inode;
    if (!ReadInode(dir_inode_nr, &dir_inode)) return 0;
    if ((dir_inode.mode & EXT2_S_IFMT) != EXT2_S_IFDIR) return 0;

    uint8_t block_buf[EXT2_MAX_BLOCK_SIZE];
    if (!ReadDirectoryFirstBlock(&dir_inode, block_buf)) return 0;

    int count = 0;
    uint32_t offset = 0;
    while (offset + sizeof(Ext2DirEntry) <= block_size && count < max_entries) {
        const Ext2DirEntry* entry = (const Ext2DirEntry*)(block_buf + offset);
        if (entry->rec_len < sizeof(Ext2DirEntry)) break; // uszkodzony/pusty blok

        const char* entry_name = (const char*)(block_buf + offset + sizeof(Ext2DirEntry));
        bool is_dot_entry = entry->name_len > 0 && entry_name[0] == '.' &&
                             (entry->name_len == 1 || (entry->name_len == 2 && entry_name[1] == '.'));

        if (entry->inode != 0 && !is_dot_entry) {
            Ext2Inode entry_inode;
            if (ReadInode(entry->inode, &entry_inode)) {
                DirEntry* de = &out_entries[count];
                uint32_t name_len = entry->name_len;
                if (name_len > FS_MAX_NAME - 1) name_len = FS_MAX_NAME - 1;
                for (uint32_t i = 0; i < name_len; i++) de->name[i] = entry_name[i];
                de->name[name_len] = '\0';
                de->size = entry_inode.size;
                de->attributes = ((entry_inode.mode & EXT2_S_IFMT) == EXT2_S_IFDIR) ? FS_ATTR_DIRECTORY : 0;
                count++;
            }
        }

        offset += entry->rec_len;
    }

    return count;
}

// Faza 1e/1f: odczyt zawartosci pliku. Do 12 blokow bezposrednich (block[0..11])
// czytane wprost z i-wezla; od 13-go bloku (Faza 1f) numery blokow pochodza z
// bloku pojedynczo posredniego (block[12] wskazuje na blok zawierajacy
// block_size/4 numerow kolejnych blokow danych, uint32 kazdy) - doczytywany co
// najwyzej raz, tylko jesli plik faktycznie tego wymaga. Podwojnie/potrojnie
// posrednie (block[13]/block[14]) nadal nieobslugiwane. Wzorowane na
// FAT32::ReadFile (alokacja przez PMM, hhdm offset), ale zamiast lancucha
// klastrow FAT iterujemy po numerach blokow ext2 wprost (nie ma odpowiednika
// FAT do przejscia).
bool Ext2::ReadFile(const char* path, uint8_t** out_buffer, uint32_t* out_size) {
    uint32_t inode_nr = 0;
    if (!ResolvePath(path, &inode_nr)) {
        SerialPort::WriteString("Ext2: ReadFile - path not found.\n");
        return false;
    }

    Ext2Inode inode;
    if (!ReadInode(inode_nr, &inode)) return false;
    if ((inode.mode & EXT2_S_IFMT) != EXT2_S_IFREG) {
        SerialPort::WriteString("Ext2: ReadFile - not a regular file.\n");
        return false;
    }

    uint32_t size = inode.size;
    if (size == 0 || block_size == 0 || block_size > EXT2_MAX_BLOCK_SIZE) return false;

    uint32_t pointers_per_block = block_size / sizeof(uint32_t);
    uint32_t blocks_needed = (size + block_size - 1) / block_size;
    if (blocks_needed > 12 + pointers_per_block) {
        SerialPort::WriteString("Ext2: ReadFile - file needs doubly/triply indirect blocks, not supported yet.\n");
        return false;
    }

    uint32_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    void* phys_ptr = PMM::AllocatePages(pages_needed);
    if (!phys_ptr) return false;

    uint8_t* v_ptr = (uint8_t*)((uint64_t)phys_ptr + hhdm_request.response->offset);

    // Blok pojedynczo posredni (jesli w ogole potrzebny) wczytywany raz do tego
    // bufora na stosie - analogiczne do block_buf w ResolvePath/ListDirectory.
    uint32_t indirect_block[EXT2_MAX_BLOCK_SIZE / sizeof(uint32_t)];
    bool indirect_loaded = false;

    uint32_t bytes_read = 0;
    for (uint32_t i = 0; i < blocks_needed; i++) {
        uint32_t block_num;
        if (i < 12) {
            block_num = inode.block[i];
        } else {
            if (!indirect_loaded) {
                if (inode.block[12] == 0) {
                    SerialPort::WriteString("Ext2: ReadFile - sparse hole (missing indirect block), not supported yet.\n");
                    PMM::FreePages(phys_ptr, pages_needed);
                    return false;
                }
                uint64_t indirect_offset = (uint64_t)inode.block[12] * block_size;
                if (!ReadDiskBytes((uint32_t)indirect_offset, block_size, (uint8_t*)indirect_block)) {
                    PMM::FreePages(phys_ptr, pages_needed);
                    return false;
                }
                indirect_loaded = true;
            }
            block_num = indirect_block[i - 12];
        }

        if (block_num == 0) {
            SerialPort::WriteString("Ext2: ReadFile - sparse hole in block, not supported yet.\n");
            PMM::FreePages(phys_ptr, pages_needed);
            return false;
        }

        uint32_t remaining = size - bytes_read;
        uint32_t read_len = (remaining < block_size) ? remaining : block_size;
        uint64_t byte_offset = (uint64_t)block_num * block_size;

        if (!ReadDiskBytes((uint32_t)byte_offset, read_len, v_ptr + bytes_read)) {
            PMM::FreePages(phys_ptr, pages_needed);
            return false;
        }
        bytes_read += read_len;
    }

    *out_buffer = v_ptr;
    *out_size = size;
    return true;
}

// Faza 2c: WriteFile dla NOWEGO pliku (plik juz istniejacy pod ta nazwa - Faza 2d,
// na razie jawnie odrzucane, nie nadpisywane). Kolejnosc krokow celowa - najpierw
// alokujemy i wypelniamy WSZYSTKO (i-wezel, bloki danych, sama tresc i-wezla), a
// dopiero na koncu dopisujemy wpis katalogowy widoczny z zewnatrz. Dzieki temu jesli
// cos zawiedzie w trakcie, katalog nigdy nie wskazuje na niekompletny/czesciowo
// zapisany plik - w najgorszym razie zostaje "cichy" wyciek zaalokowanego i-wezla/
// blokow (do naprawy przez `e2fsck -f` na hoscie, tak jak dzis wszystkie inne
// nieobsluzone awarie dysku w tym sterowniku - FAT32::WriteFile tez nie robi
// pelnego rollbacku na kazdym mozliwym I/O-error, patrz fat32.cpp). Tylko pliki
// miesczace sie w 12 blokach bezposrednich (jak Faza 1e dla odczytu, przed Faza 1f)
// - wieksze pliki wymagalyby jednoczesnej alokacji bloku posredniego, zostawione na
// pozniej jesli sie okaze potrzebne (dzis najwiekszy zapisywany w runtime plik to
// /notatka.txt z Notatnika, daleko ponizej tego limitu).
bool Ext2::WriteFile(const char* path, const uint8_t* buffer, uint32_t size) {
    if (block_size == 0 || block_size > EXT2_MAX_BLOCK_SIZE || group_desc_table == nullptr) return false;

    char parent_path[256];
    char filename[256];
    uint32_t name_len = 0;
    if (!SplitParentAndName(path, parent_path, filename, &name_len)) {
        SerialPort::WriteString("Ext2: WriteFile - invalid path.\n");
        return false;
    }

    uint32_t parent_inode_nr = 0;
    if (!ResolvePath(parent_path, &parent_inode_nr)) {
        SerialPort::WriteString("Ext2: WriteFile - parent directory not found.\n");
        return false;
    }

    Ext2Inode parent_inode;
    if (!ReadInode(parent_inode_nr, &parent_inode)) return false;
    if ((parent_inode.mode & EXT2_S_IFMT) != EXT2_S_IFDIR) {
        SerialPort::WriteString("Ext2: WriteFile - parent is not a directory.\n");
        return false;
    }

    uint8_t block_buf[EXT2_MAX_BLOCK_SIZE];
    if (!ReadDirectoryFirstBlock(&parent_inode, block_buf)) return false;

    uint32_t existing_inode = 0;
    if (FindEntryInBlock(block_buf, block_size, filename, name_len, &existing_inode)) {
        SerialPort::WriteString("Ext2: WriteFile - file already exists, overwrite not supported yet (Faza 2d).\n");
        return false;
    }

    uint32_t blocks_needed = (size + block_size - 1) / block_size;
    if (blocks_needed > 12) {
        SerialPort::WriteString("Ext2: WriteFile - file needs indirect blocks, not supported yet for new files.\n");
        return false;
    }

    uint32_t needed_entry_len = (sizeof(Ext2DirEntry) + name_len + 3) & ~3u;
    uint32_t insert_offset = 0;
    uint16_t insert_rec_len = 0;
    if (!FindDirInsertSlot(block_buf, block_size, needed_entry_len, &insert_offset, &insert_rec_len)) {
        SerialPort::WriteString("Ext2: WriteFile - no room for new directory entry in parent's first block.\n");
        return false;
    }

    uint32_t new_inode_nr = 0;
    if (!AllocateInode(&new_inode_nr)) {
        SerialPort::WriteString("Ext2: WriteFile - no free inodes.\n");
        return false;
    }

    uint32_t data_blocks[12] = {0};
    for (uint32_t i = 0; i < blocks_needed; i++) {
        if (!AllocateBlock(&data_blocks[i])) {
            for (uint32_t j = 0; j < i; j++) FreeBlock(data_blocks[j]);
            FreeInode(new_inode_nr);
            SerialPort::WriteString("Ext2: WriteFile - no free blocks.\n");
            return false;
        }
    }

    uint32_t bytes_written = 0;
    for (uint32_t i = 0; i < blocks_needed; i++) {
        uint32_t chunk = block_size;
        if (size - bytes_written < block_size) chunk = size - bytes_written;
        uint64_t byte_offset = (uint64_t)data_blocks[i] * block_size;
        if (!WriteDiskBytes((uint32_t)byte_offset, chunk, buffer + bytes_written)) {
            for (uint32_t j = 0; j < blocks_needed; j++) FreeBlock(data_blocks[j]);
            FreeInode(new_inode_nr);
            SerialPort::WriteString("Ext2: WriteFile - disk write failed while writing data blocks.\n");
            return false;
        }
        bytes_written += chunk;
    }

    Ext2Inode new_inode = {};
    new_inode.mode = EXT2_S_IFREG | 0644;
    new_inode.size = size;
    new_inode.links_count = 1;
    new_inode.blocks = blocks_needed * (block_size / 512); // pole "blocks" liczy sektory 512B, nie bloki
    for (uint32_t i = 0; i < blocks_needed; i++) new_inode.block[i] = data_blocks[i];

    if (!WriteInode(new_inode_nr, &new_inode)) {
        for (uint32_t j = 0; j < blocks_needed; j++) FreeBlock(data_blocks[j]);
        FreeInode(new_inode_nr);
        SerialPort::WriteString("Ext2: WriteFile - failed to write new inode.\n");
        return false;
    }

    Ext2DirEntry* new_entry = (Ext2DirEntry*)(block_buf + insert_offset);
    new_entry->inode = new_inode_nr;
    new_entry->rec_len = insert_rec_len;
    new_entry->name_len = (uint8_t)name_len;
    new_entry->file_type = EXT2_FT_REG_FILE;
    uint8_t* name_dst = block_buf + insert_offset + sizeof(Ext2DirEntry);
    for (uint32_t i = 0; i < name_len; i++) name_dst[i] = filename[i];

    if (!WriteDirectoryFirstBlock(&parent_inode, block_buf)) {
        SerialPort::WriteString("Ext2: WriteFile - failed to write updated directory block.\n");
        return false;
    }

    SerialPort::WriteString("Ext2: File written successfully.\n");
    return true;
}

// Faza 1e: analogicznie do FAT32::FreeFile - zamienia adres wirtualny (hhdm) z
// powrotem na fizyczny i zwraca strony do PMM.
void Ext2::FreeFile(uint8_t* buffer, uint32_t size) {
    if (!buffer) return;
    void* phys_ptr = (void*)((uint64_t)buffer - hhdm_request.response->offset);
    uint32_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    PMM::FreePages(phys_ptr, pages);
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
    bgdt_block = superblock.first_data_block + 1;
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

    // Faza 1d: test ListDirectory("/") - do reczne porownania z
    // `debugfs -R "ls -l /" test.img` na hoscie (te same nazwy/rozmiary/atrybuty,
    // bez "."/"..", weryfikacja Fazy 1d).
    DirEntry root_entries[32];
    int root_count = ListDirectory("/", root_entries, 32);
    SerialPort::WriteString("Ext2: ListDirectory(/) -> "); print_uint32(root_count);
    SerialPort::WriteString(" entries\n");
    for (int i = 0; i < root_count; i++) {
        SerialPort::WriteString("Ext2:   ");
        SerialPort::WriteString(root_entries[i].name);
        SerialPort::WriteString(root_entries[i].attributes & FS_ATTR_DIRECTORY ? " [DIR]" : " [FILE]");
        SerialPort::WriteString(" size="); print_uint32(root_entries[i].size);
        SerialPort::WriteString("\n");
    }

    // Faza 1e: test ReadFile("/hello.txt") - plik testowy dopisywany recznie przez
    // `debugfs -w` na obrazie mke2fs (patrz raport tej fazy), wiec na "gorym" obrazie
    // bez tego kroku ResolvePath po prostu nie znajdzie sciezki i test zaloguje "not
    // found" - bezpieczne, analogicznie do testu ResolvePath(/lost+found) w Fazie 1c
    // gdy dysk slave w ogole nie jest podpiety. Do reczne porownania zawartosci z
    // `debugfs -R "cat hello.txt" test.img` na hoscie.
    uint8_t* file_buf = nullptr;
    uint32_t file_size = 0;
    if (ReadFile("/hello.txt", &file_buf, &file_size)) {
        SerialPort::WriteString("Ext2: ReadFile(/hello.txt) -> "); print_uint32(file_size);
        SerialPort::WriteString(" bytes: \"");
        for (uint32_t i = 0; i < file_size; i++) SerialPort::WriteChar((char)file_buf[i]);
        SerialPort::WriteString("\"\n");
        FreeFile(file_buf, file_size);
    } else {
        SerialPort::WriteString("Ext2: ReadFile(/hello.txt) failed (expected on images without the test file).\n");
    }

}
