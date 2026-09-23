#pragma once
#include <stdint.h>
#include "dirent.h"

// Faza 1a/1b/1c (patrz CoworkWithClaude/PLAN_ext2_filesystem.md): na razie tylko Init()
// parsuje superblok + tablice deskryptorow grup blokow, (Faza 1b) odczytuje i loguje
// i-wezel root jako test, (Faza 1c) rozwiazuje testowa sciezke "/lost+found" przez
// nowa ResolvePath. Odczyt i-wezla (Ext2Inode/ReadInode) i rozwiazywanie sciezek
// (ResolvePath, parser wpisow katalogowych) zyja na razie tylko w ext2.cpp - beda
// potrzebne reszcie Fazy 1 (1d: ListDirectory, 1e/1f: ReadFile), wtedy przeniesie
// sie/rozszerzy w miare potrzeb. Sterownik NIE jest jeszcze podlaczony do VFS:: -
// disk.img zostaje FAT32-owy, testujemy na osobnym obrazie mke2fs podlaczonym jako
// drugi dysk QEMU (patrz ext2.cpp).
class Ext2 {
public:
    static void Init();
    static bool ReadFile(const char* path, uint8_t** out_buffer, uint32_t* out_size);
    static bool WriteFile(const char* path, const uint8_t* buffer, uint32_t size);
    static void FreeFile(uint8_t* buffer, uint32_t size);
    static int ListDirectory(const char* path, DirEntry* out_entries, int max_entries);
};
