#pragma once
#include <stdint.h>
#include "dirent.h"

// Faza 1a (patrz CoworkWithClaude/PLAN_ext2_filesystem.md): na razie tylko Init()
// parsuje superblok + tablice deskryptorow grup blokow i loguje wynik. Sterownik
// NIE jest jeszcze podlaczony do VFS:: - disk.img zostaje FAT32-owy, testujemy na
// osobnym obrazie mke2fs podlaczonym jako drugi dysk QEMU (patrz ext2.cpp).
class Ext2 {
public:
    static void Init();
    static bool ReadFile(const char* path, uint8_t** out_buffer, uint32_t* out_size);
    static bool WriteFile(const char* path, const uint8_t* buffer, uint32_t size);
    static void FreeFile(uint8_t* buffer, uint32_t size);
    static int ListDirectory(const char* path, DirEntry* out_entries, int max_entries);
};
