#include "vfs.h"
#include "fat32.h"
#include "../serial.h"

void VFS::Init() {
    SerialPort::WriteString("VFS: Initializing Virtual File System...\n");
    FAT32::Init();
}

bool VFS::ReadFile(const char* path, uint8_t** out_buffer, uint32_t* out_size) {
    if (path[0] == '/') {
        path++; // Omiń początkowy ukośnik
    }
    
    return FAT32::ReadFile(path, out_buffer, out_size);
}
