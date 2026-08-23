#pragma once
#include <stdint.h>

class FAT32 {
public:
    static void Init();
    static bool ReadFile(const char* path, uint8_t** out_buffer, uint32_t* out_size);
    static bool WriteFile(const char* path, const uint8_t* buffer, uint32_t size);
    static void FreeFile(uint8_t* buffer, uint32_t size);
};
