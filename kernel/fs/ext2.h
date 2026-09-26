#pragma once
#include <stdint.h>
#include "dirent.h"

// Faza 1a/1b/1c/1d/1e/1f (patrz CoworkWithClaude/PLAN_ext2_filesystem.md): Init() parsuje
// superblok + tablice deskryptorow grup blokow, (Faza 1b) odczytuje i loguje i-wezel
// root jako test, (Faza 1c) rozwiazuje testowa sciezke "/lost+found" przez ResolvePath,
// (Faza 1d) ListDirectory jest pierwsza publiczna funkcja Ext2:: z prawdziwa
// implementacja, (Faza 1e) ReadFile odczytuje pliki miesczace sie w 12 blokach
// bezposrednich (block[0..11]), (Faza 1f) rozszerzone o odczyt przez blok pojedynczo
// posredni (block[12]) dla plikow wiekszych - podwojnie/potrojnie posredni
// (block[13]/block[14]) wciaz nieobslugiwane. WriteFile na razie sama deklaracja
// (Faza 2). Odczyt i-wezla (Ext2Inode/ReadInode) i rozwiazywanie sciezek (ResolvePath,
// parser wpisow katalogowych) zyja na razie tylko w ext2.cpp. Sterownik NIE jest
// jeszcze podlaczony do VFS:: - disk.img zostaje FAT32-owy, testujemy na osobnym
// obrazie mke2fs podlaczonym jako drugi dysk QEMU (patrz ext2.cpp).
class Ext2 {
public:
    static void Init();
    static bool ReadFile(const char* path, uint8_t** out_buffer, uint32_t* out_size);
    static bool WriteFile(const char* path, const uint8_t* buffer, uint32_t size);
    static void FreeFile(uint8_t* buffer, uint32_t size);
    static int ListDirectory(const char* path, DirEntry* out_entries, int max_entries);
};
