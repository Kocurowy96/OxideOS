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

static uint32_t GetClusterLBA(uint32_t cluster) {
    return data_start_lba + (cluster - 2) * sectors_per_cluster;
}

static uint32_t GetNextCluster(uint32_t current_cluster) {
    uint32_t fat_offset = current_cluster * 4;
    uint32_t fat_sector = fat_start_lba + (fat_offset / 512);
    uint32_t ent_offset = fat_offset % 512;
    
    uint8_t sector[512];
    if (!ATA::ReadSector(fat_sector, sector)) return 0x0FFFFFFF;
    
    uint32_t next = *(uint32_t*)&sector[ent_offset];
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

bool FAT32::ReadFile(const char* path, uint8_t** out_buffer, uint32_t* out_size) {
    if (bytes_per_sector != 512) return false;
    
    uint32_t current_cluster = root_cluster;
    uint8_t sector[512];
    
    uint32_t target_cluster = 0;
    uint32_t file_size = 0;
    
    // Szukanie w Root Directory
    while (current_cluster < 0x0FFFFFF8) {
        uint32_t lba = GetClusterLBA(current_cluster);
        
        for (int i = 0; i < sectors_per_cluster; i++) {
            if (!ATA::ReadSector(lba + i, sector)) return false;
            
            for (int entry = 0; entry < 512; entry += 32) {
                if (sector[entry] == 0x00) break; // Koniec wpisów
                if (sector[entry] == 0xE5) continue; // Usunięty
                if (sector[entry + 11] == 0x0F) continue; // LFN
                if (sector[entry + 11] & 0x08) continue; // Volume ID
                if (str_eq_83((const char*)&sector[entry], path)) {
                    target_cluster = ((uint32_t)*(uint16_t*)&sector[entry + 20] << 16) | *(uint16_t*)&sector[entry + 26];
                    file_size = *(uint32_t*)&sector[entry + 28];
                    break;
                }
            }
            if (target_cluster) break;
        }
        
        if (target_cluster) break;
        current_cluster = GetNextCluster(current_cluster);
    }
    
    if (!target_cluster || !file_size) {
        SerialPort::WriteString("FAT32: File not found.\n");
        return false;
    }
    
    // Alokacja bufora
    uint32_t pages_needed = (file_size + 4095) / 4096;
    void* phys_ptr = PMM::AllocatePages(pages_needed);
    if (!phys_ptr) return false;
    
    uint8_t* v_ptr = (uint8_t*)((uint64_t)phys_ptr + hhdm_request.response->offset);
    *out_buffer = v_ptr;
    *out_size = file_size;
    
    // Odczyt zawartości pliku
    uint32_t bytes_read = 0;
    current_cluster = target_cluster;
    
    while (current_cluster < 0x0FFFFFF8 && bytes_read < file_size) {
        uint32_t lba = GetClusterLBA(current_cluster);
        for (int i = 0; i < sectors_per_cluster && bytes_read < file_size; i++) {
            if (!ATA::ReadSector(lba + i, sector)) return false;
            
            uint32_t to_copy = 512;
            if (file_size - bytes_read < 512) to_copy = file_size - bytes_read;
            
            mem_cpy(v_ptr + bytes_read, sector, to_copy);
            bytes_read += to_copy;
        }
        current_cluster = GetNextCluster(current_cluster);
    }
    
    SerialPort::WriteString("FAT32: File loaded successfully.\n");
    return true;
}
