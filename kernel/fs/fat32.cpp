#include "fat32.h"
#include "../drivers/ata.h"
#include "../serial.h"
#include "../mem/pmm.h"
#include "../limine.h"

extern volatile struct limine_hhdm_request hhdm_request;

static uint32_t partition_start_lba = 0;
static uint16_t bytes_per_sector = 512;
static uint8_t sectors_per_cluster = 0;
static uint16_t reserved_sectors = 0;
static uint8_t num_fats = 0;
static uint32_t sectors_per_fat = 0;
static uint32_t root_cluster = 0;

static uint32_t fat_start_lba = 0;
static uint32_t data_start_lba = 0;

static void mem_cpy(void* dest, const void* src, uint32_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) *d++ = *s++;
}

static bool str_eq_83(const char* name_83, const char* search_path) {
    char formatted[11];
    for (int i = 0; i < 11; i++) formatted[i] = ' ';
    
    int i = 0, j = 0;
    while (search_path[i] && search_path[i] != '.' && j < 8) {
        char c = search_path[i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        formatted[j++] = c;
    }
    if (search_path[i] == '.') {
        i++;
        j = 8;
        while (search_path[i] && j < 11) {
            char c = search_path[i++];
            if (c >= 'a' && c <= 'z') c -= 32;
            formatted[j++] = c;
        }
    }
    
    for (int k = 0; k < 11; k++) {
        if (name_83[k] != formatted[k]) return false;
    }
    return true;
}

// ---- LFN (Long File Name) helpers ----

// Wyciąga 13 znaków UTF-16LE z wpisu LFN i zapisuje jako ASCII do buf[offset..offset+13]
static void extract_lfn_chars(const uint8_t* entry, char* buf, int offset) {
    // Offsets w 32-bajtowym wpisie LFN: 1-10 (5 znaków), 14-25 (6 znaków), 28-31 (2 znaki)
    static const int lfn_offsets[] = {1,3,5,7,9, 14,16,18,20,22,24, 28,30};
    for (int i = 0; i < 13; i++) {
        uint16_t ch = (uint16_t)entry[lfn_offsets[i]] | ((uint16_t)entry[lfn_offsets[i]+1] << 8);
        if (ch == 0xFFFF || ch == 0x0000) {
            buf[offset + i] = '\0';
        } else if (ch < 128) {
            buf[offset + i] = (char)ch;
        } else {
            buf[offset + i] = '?'; // znaki spoza ASCII
        }
    }
}

// Porównuje zebrany LFN z szukaną nazwą (case-insensitive)
static bool str_eq_lfn(const char* lfn, const char* search) {
    int i = 0;
    while (lfn[i] != '\0' && search[i] != '\0') {
        char a = lfn[i], b = search[i];
        if (a >= 'A' && a <= 'Z') a += 32;
        if (b >= 'A' && b <= 'Z') b += 32;
        if (a != b) return false;
        i++;
    }
    return lfn[i] == '\0' && search[i] == '\0';
}

// Bufor na zebrany LFN (max 255 znaków + null)
static char lfn_buf[256];
static int  lfn_entries_collected = 0;

// Inicjuje bufor LFN przed skanowaniem katalogu
static void lfn_reset() {
    for (int i = 0; i < 256; i++) lfn_buf[i] = '\0';
    lfn_entries_collected = 0;
}

// Dodaje jeden wpis LFN do bufora
static void lfn_add_entry(const uint8_t* entry) {
    int seq = entry[0] & 0x3F; // numer wpisu (1-based)
    int offset = (seq - 1) * 13;
    if (offset >= 0 && offset + 13 <= 255) {
        extract_lfn_chars(entry, lfn_buf, offset);
        lfn_entries_collected++;
    }
}

// Sprawdza czy zebrany LFN pasuje do szukanej nazwy
static bool lfn_matches(const char* search) {
    if (lfn_entries_collected == 0) return false;
    return str_eq_lfn(lfn_buf, search);
}

void FAT32::FreeFile(uint8_t* buffer, uint32_t size) {
    if (!buffer) return;
    void* phys_ptr = (void*)((uint64_t)buffer - hhdm_request.response->offset);
    uint32_t pages = (size + 4095) / 4096;
    PMM::FreePages(phys_ptr, pages);
}

static uint32_t GetClusterLBA(uint32_t cluster) {
    return data_start_lba + (cluster - 2) * sectors_per_cluster;
}

static uint32_t GetNextCluster(uint32_t current_cluster) {
    uint32_t fat_offset = current_cluster * 4;
    uint32_t fat_sector = fat_start_lba + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;
    
    static uint32_t cached_fat_sector = 0xFFFFFFFF;
    static uint8_t cached_fat_data[512];
    
    if (cached_fat_sector != fat_sector) {
        if (!ATA::ReadSector(fat_sector, cached_fat_data)) return 0x0FFFFFFF;
        cached_fat_sector = fat_sector;
    }
    
    uint32_t next = *(uint32_t*)&cached_fat_data[ent_offset];
    return next & 0x0FFFFFFF;
}

void FAT32::Init() {
    uint8_t sector[512];
    if (!ATA::ReadSector(0, sector)) {
        SerialPort::WriteString("FAT32: Failed to read MBR.\n");
        return;
    }
    
    // MBR partition 1
    if (sector[510] != 0x55 || sector[511] != 0xAA) {
        SerialPort::WriteString("FAT32: Invalid MBR signature.\n");
        return;
    }
    
    // Sprawdź czy to od razu Boot Sector (Superfloppy) czy MBR
    // W FAT32 na offsecie 82 często jest napis "FAT32   ", ale bezpieczniej 
    // jest założyć, że jeśli to nie jest MBR (brak prawidłowej partycji), to start_lba = 0.
    partition_start_lba = *(uint32_t*)&sector[0x1BE + 8];
    uint8_t type = sector[0x1BE + 4];
    
    if (type != 0x0B && type != 0x0C) { // Not FAT32
        // Próba Superfloppy (cały dysk to FAT32)
        partition_start_lba = 0;
    }
    
    if (partition_start_lba != 0) {
        if (!ATA::ReadSector(partition_start_lba, sector)) {
            SerialPort::WriteString("FAT32: Failed to read Boot Sector.\n");
            return;
        }
    }
    
    bytes_per_sector = *(uint16_t*)&sector[11];
    sectors_per_cluster = sector[13];
    reserved_sectors = *(uint16_t*)&sector[14];
    num_fats = sector[16];
    sectors_per_fat = *(uint32_t*)&sector[36];
    root_cluster = *(uint32_t*)&sector[44];
    
    if (bytes_per_sector != 512 || sectors_per_cluster == 0 || num_fats == 0) {
        SerialPort::WriteString("FAT32: Invalid FAT32 Boot Sector.\n");
        return;
    }
    
    fat_start_lba = partition_start_lba + reserved_sectors;
    data_start_lba = fat_start_lba + (num_fats * sectors_per_fat);
    
    SerialPort::WriteString("FAT32: Initialized successfully.\n");
}

static uint32_t FindDirectoryCluster(const char* path, char* filename) {
    uint32_t current_cluster = root_cluster;
    int path_idx = 0;
    
    while (path[path_idx] != '\0') {
        char token[128];
        int token_idx = 0;
        while (path[path_idx] != '\0' && path[path_idx] != '/') {
            token[token_idx++] = path[path_idx++];
        }
        token[token_idx] = '\0';
        
        if (path[path_idx] == '\0') {
            for(int i=0; i<=token_idx; i++) filename[i] = token[i];
            return current_cluster;
        }
        
        path_idx++;
        
        uint32_t next_cluster = 0;
        uint32_t iter_cluster = current_cluster;
        uint8_t sector[512];
        
        while (iter_cluster < 0x0FFFFFF8) {
            uint32_t lba = GetClusterLBA(iter_cluster);
            for (int i = 0; i < sectors_per_cluster; i++) {
                if (!ATA::ReadSector(lba + i, sector)) return 0;
                lfn_reset();
                for (int entry = 0; entry < 512; entry += 32) {
                    if (sector[entry] == 0x00) break;
                    if (sector[entry] == 0xE5) { lfn_reset(); continue; }
                    if (sector[entry + 11] == 0x0F) { lfn_add_entry(&sector[entry]); continue; }
                    if (!(sector[entry + 11] & 0x10)) { lfn_reset(); continue; }
                    
                    if (lfn_matches(token) || str_eq_83((const char*)&sector[entry], token)) {
                        next_cluster = ((uint32_t)*(uint16_t*)&sector[entry + 20] << 16) | *(uint16_t*)&sector[entry + 26];
                        break;
                    }
                    lfn_reset();
                }
                if (next_cluster) break;
                
                bool has_zero = false;
                for (int entry = 0; entry < 512; entry += 32) {
                    if (sector[entry] == 0x00) { has_zero = true; break; }
                }
                if (has_zero) break;
                lfn_reset();
            }
            if (next_cluster) break;
            iter_cluster = GetNextCluster(iter_cluster);
        }
        
        if (!next_cluster) return 0;
        current_cluster = next_cluster;
    }
    
    return current_cluster;
}

static uint32_t FindFreeCluster() {
    uint8_t sector[512];
    for(uint32_t fat_sec = 0; fat_sec < sectors_per_fat; fat_sec++) {
        if (!ATA::ReadSector(fat_start_lba + fat_sec, sector)) return 0;
        for(int i=0; i<512; i+=4) {
            uint32_t val = *(uint32_t*)&sector[i];
            if ((val & 0x0FFFFFFF) == 0) {
                uint32_t cluster = (fat_sec * 512 + i) / 4;
                if (cluster >= 2) return cluster;
            }
        }
    }
    return 0;
}

static void SetFATEntry(uint32_t cluster, uint32_t val) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_start_lba + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;
    
    uint8_t sector[512];
    if (ATA::ReadSector(fat_sector, sector)) {
        *(uint32_t*)&sector[ent_offset] = val;
        ATA::WriteSector(fat_sector, sector);
        if (num_fats > 1) {
            ATA::WriteSector(fat_sector + sectors_per_fat, sector);
        }
    }
}

bool FAT32::ReadFile(const char* path, uint8_t** out_buffer, uint32_t* out_size) {
    if (bytes_per_sector != 512) return false;
    
    char filename[128];
    uint32_t dir_cluster = FindDirectoryCluster(path, filename);
    if (!dir_cluster) {
        SerialPort::WriteString("FAT32: Directory not found.\n");
        return false;
    }
    
    uint32_t target_cluster = 0;
    uint32_t file_size = 0;
    uint32_t current_cluster = dir_cluster;
    uint8_t sector[512];
    
    while (current_cluster < 0x0FFFFFF8) {
        uint32_t lba = GetClusterLBA(current_cluster);
        for (int i = 0; i < sectors_per_cluster; i++) {
            if (!ATA::ReadSector(lba + i, sector)) return false;
            lfn_reset();
            for (int entry = 0; entry < 512; entry += 32) {
                if (sector[entry] == 0x00) break;
                if (sector[entry] == 0xE5) { lfn_reset(); continue; }
                // Wpis LFN - zbierz
                if (sector[entry + 11] == 0x0F) { lfn_add_entry(&sector[entry]); continue; }
                // Pomiń wpisy woluminowe i katalogowe
                if (sector[entry + 11] & 0x08) { lfn_reset(); continue; }
                if (sector[entry + 11] & 0x10) { lfn_reset(); continue; }
                
                if (lfn_matches(filename) || str_eq_83((const char*)&sector[entry], filename)) {
                    target_cluster = ((uint32_t)*(uint16_t*)&sector[entry + 20] << 16) | *(uint16_t*)&sector[entry + 26];
                    file_size = *(uint32_t*)&sector[entry + 28];
                    break;
                }
                lfn_reset();
            }
            if (target_cluster) break;
            
            bool has_zero = false;
            for (int entry = 0; entry < 512; entry += 32) {
                if (sector[entry] == 0x00) { has_zero = true; break; }
            }
            if (has_zero) break;
            lfn_reset();
        }
        if (target_cluster) break;
        current_cluster = GetNextCluster(current_cluster);
    }
    
    if (!target_cluster || !file_size) {
        SerialPort::WriteString("FAT32: File not found.\n");
        return false;
    }
    
    uint32_t pages_needed = (file_size + 4095) / 4096;
    void* phys_ptr = PMM::AllocatePages(pages_needed);
    if (!phys_ptr) return false;
    
    uint8_t* v_ptr = (uint8_t*)((uint64_t)phys_ptr + hhdm_request.response->offset);
    *out_buffer = v_ptr;
    *out_size = file_size;
    
    uint32_t bytes_read = 0;
    current_cluster = target_cluster;
    
    while (current_cluster < 0x0FFFFFF8 && bytes_read < file_size) {
        uint32_t start_cluster = current_cluster;
        uint32_t num_clusters = 1;
        uint32_t next_cluster = GetNextCluster(current_cluster);
        
        // Merge contiguous clusters (up to 255 sectors total)
        while (next_cluster == current_cluster + 1 && next_cluster < 0x0FFFFFF8 && (num_clusters + 1) * sectors_per_cluster <= 255) {
            num_clusters++;
            current_cluster = next_cluster;
            next_cluster = GetNextCluster(current_cluster);
        }
        
        uint32_t lba = GetClusterLBA(start_cluster);
        uint32_t bytes_left = file_size - bytes_read;
        uint32_t sectors_to_read = num_clusters * sectors_per_cluster;
        
        if (bytes_left < sectors_to_read * 512) {
            sectors_to_read = (bytes_left + 511) / 512;
        }
        
        // Read directly into virtual memory (safe because we allocated page-aligned memory)
        if (!ATA::ReadSectors(lba, sectors_to_read, v_ptr + bytes_read)) return false;
        
        if (bytes_left >= sectors_to_read * 512) {
            bytes_read += sectors_to_read * 512;
        } else {
            bytes_read += bytes_left;
        }
        
        current_cluster = next_cluster;
    }
    
    SerialPort::WriteString("FAT32: File loaded successfully.\n");
    return true;
}

bool FAT32::WriteFile(const char* path, const uint8_t* buffer, uint32_t size) {
    if (bytes_per_sector != 512) return false;
    
    char filename[128];
    uint32_t dir_cluster = FindDirectoryCluster(path, filename);
    if (!dir_cluster) {
        SerialPort::WriteString("FAT32: Directory not found.\n");
        return false;
    }
    
    uint8_t sector[512];
    uint32_t target_cluster = 0;
    uint32_t dir_entry_lba = 0;
    uint32_t dir_entry_offset = 0;
    uint32_t empty_lba = 0;
    uint32_t empty_offset = 0;
    
    uint32_t iter_cluster = dir_cluster;
    
    while (iter_cluster < 0x0FFFFFF8) {
        uint32_t lba = GetClusterLBA(iter_cluster);
        for (int i = 0; i < sectors_per_cluster; i++) {
            if (!ATA::ReadSector(lba + i, sector)) return false;
            lfn_reset();
            for (int entry = 0; entry < 512; entry += 32) {
                if (sector[entry] == 0x00 || sector[entry] == 0xE5) {
                    if (empty_lba == 0 && (sector[entry] == 0x00 || sector[entry] == 0xE5)) {
                        empty_lba = lba + i;
                        empty_offset = entry;
                    }
                    if (sector[entry] == 0x00) break;
                    lfn_reset();
                    continue;
                }
                // Wpis LFN - zbierz
                if (sector[entry + 11] == 0x0F) { lfn_add_entry(&sector[entry]); continue; }
                if (sector[entry + 11] & 0x08) { lfn_reset(); continue; }
                if (sector[entry + 11] & 0x10) { lfn_reset(); continue; }
                
                if (lfn_matches(filename) || str_eq_83((const char*)&sector[entry], filename)) {
                    target_cluster = ((uint32_t)*(uint16_t*)&sector[entry + 20] << 16) | *(uint16_t*)&sector[entry + 26];
                    dir_entry_lba = lba + i;
                    dir_entry_offset = entry;
                    break;
                }
                lfn_reset();
            }
            if (target_cluster) break;
            
            bool has_zero = false;
            for (int entry = 0; entry < 512; entry += 32) {
                if (sector[entry] == 0x00) { has_zero = true; break; }
            }
            if (has_zero) break;
            lfn_reset();
        }
        if (target_cluster) break;
        iter_cluster = GetNextCluster(iter_cluster);
    }
    
    if (!target_cluster) {
        if (empty_lba == 0) {
            SerialPort::WriteString("FAT32: No empty directory entries.\n");
            return false;
        }
        
        target_cluster = FindFreeCluster();
        if (!target_cluster) return false;
        SetFATEntry(target_cluster, 0x0FFFFFFF);
        
        dir_entry_lba = empty_lba;
        dir_entry_offset = empty_offset;
        
        if (!ATA::ReadSector(dir_entry_lba, sector)) return false;
        
        char formatted[11];
        for (int i = 0; i < 11; i++) formatted[i] = ' ';
        int i = 0, j = 0;
        while (filename[i] && filename[i] != '.' && j < 8) {
            char c = filename[i++];
            if (c >= 'a' && c <= 'z') c -= 32;
            formatted[j++] = c;
        }
        if (filename[i] == '.') {
            i++; j = 8;
            while (filename[i] && j < 11) {
                char c = filename[i++];
                if (c >= 'a' && c <= 'z') c -= 32;
                formatted[j++] = c;
            }
        }
        
        for(int k=0; k<11; k++) sector[dir_entry_offset + k] = formatted[k];
        sector[dir_entry_offset + 11] = 0x20; 
        for(int k=12; k<32; k++) sector[dir_entry_offset + k] = 0;
        
        *(uint16_t*)&sector[dir_entry_offset + 20] = (uint16_t)(target_cluster >> 16);
        *(uint16_t*)&sector[dir_entry_offset + 26] = (uint16_t)(target_cluster & 0xFFFF);
        *(uint32_t*)&sector[dir_entry_offset + 28] = size;
        
        ATA::WriteSector(dir_entry_lba, sector);
    } else {
        if (ATA::ReadSector(dir_entry_lba, sector)) {
            *(uint32_t*)&sector[dir_entry_offset + 28] = size;
            ATA::WriteSector(dir_entry_lba, sector);
        }
    }
    
    uint32_t bytes_written = 0;
    uint32_t current_cluster = target_cluster;
    
    while (bytes_written < size) {
        uint32_t lba = GetClusterLBA(current_cluster);
        for (int i = 0; i < sectors_per_cluster && bytes_written < size; i++) {
            uint8_t write_buf[512] = {0};
            uint32_t to_copy = 512;
            if (size - bytes_written < 512) to_copy = size - bytes_written;
            
            mem_cpy(write_buf, buffer + bytes_written, to_copy);
            ATA::WriteSector(lba + i, write_buf);
            bytes_written += to_copy;
        }
        
        if (bytes_written < size) {
            uint32_t next = GetNextCluster(current_cluster);
            if (next >= 0x0FFFFFF8) {
                uint32_t new_cluster = FindFreeCluster();
                if (!new_cluster) return false;
                SetFATEntry(current_cluster, new_cluster);
                SetFATEntry(new_cluster, 0x0FFFFFFF);
                current_cluster = new_cluster;
            } else {
                current_cluster = next;
            }
        }
    }
    
    SerialPort::WriteString("FAT32: File written successfully.\n");
    return true;
}
