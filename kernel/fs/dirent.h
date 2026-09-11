#pragma once
#include <stdint.h>

#define FS_MAX_NAME 256
#define FS_ATTR_DIRECTORY 0x01

struct DirEntry {
    char name[FS_MAX_NAME];
    uint32_t size;
    uint8_t attributes; // bit 0 (FS_ATTR_DIRECTORY) = jest katalogiem
};
