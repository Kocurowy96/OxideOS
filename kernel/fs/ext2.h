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
// (block[13]/block[14]) wciaz nieobslugiwane. (Faza 2a) doszla bitmapa wolnych blokow
// (AllocateBlock/FreeBlock), (Faza 2b) analogiczna bitmapa wolnych i-wezlow
// (AllocateInode/FreeInode), (Faza 2c) WriteFile dziala dla NOWEGO pliku (alokacja
// i-wezla+blokow bezposrednich, zapis danych, nowy wpis katalogowy w pierwszym
// bloku katalogu-rodzica), (Faza 2d) WriteFile dziala tez dla JUZ ISTNIEJACEGO
// pliku (OverwriteExistingFile - powieksza/skraca liste blokow bezposrednich i
// aktualizuje size, wpis katalogowy sie nie zmienia). Cala Faza 2 (odczyt+zapis)
// jest teraz kompletna dla plikow miesczacych sie w 12 blokach bezposrednich -
// wieksze pliki (wymagajace bloku posredniego przy zapisie) nadal odrzucane, tak
// jak podwojnie/potrojnie posrednie przy odczycie. Odczyt i-wezla (Ext2Inode/
// ReadInode) i rozwiazywanie sciezek (ResolvePath, parser wpisow katalogowych) zyja
// na razie tylko w ext2.cpp.
//
// (Faza 3a) scripts/make_disk.sh buduje disk.img przez mke2fs zamiast mtools/FAT32.
// (Faza 3b) kernel/fs/vfs.cpp wola teraz Ext2:: zamiast FAT32:: - disk.img to
// PRODUKCYJNY obraz na primary master (-hda), wiec ReadDiskSector/WriteDiskSector
// w ext2.cpp przelaczone z ATA::...Slave (testowe, Fazy 1-2, -hdb) na zwykle
// ATA::ReadSector/WriteSector. Init() nie robi juz zadnej diagnostyki poza
// parsowaniem superbloku/BGDT (Fazy 1a-1e-owe testy ResolvePath/ListDirectory/
// ReadFile usuniete z Init() - byly pomocami deweloperskimi do reczne porownania
// z hostowym debugfs, niepotrzebne/nieodpowiednie na kazdym prawdziwym boocie
// produkcyjnym). Zweryfikowane w Fazie 3c pelnym regresem (wszystkie 9 apek z
// /usr/bin, wiele okien Ring3 na raz, zapis/odczyt tapety i Notatnika przez
// prawdziwe syscalle, e2fsck -f czysty) - patrz TASKS.md. FAT32:: (fat32.cpp)
// zostaje w drzewie nietkniete, ale VFS:: juz go nie wywoluje - usuniecie to
// decyzja wlasciciela repo, nie podjeta automatycznie w tej sesji.
//
// Uwaga: kernel/proc/sched.cpp - per-task kernel stack zwiekszony z 2 na 8 stron
// przy okazji Fazy 2c (WriteFile+ResolvePath razem trzymaja na stosie wiecej niz
// stary budzet 8KiB pozwalal), patrz komentarz tam.
class Ext2 {
public:
    static void Init();
    static bool ReadFile(const char* path, uint8_t** out_buffer, uint32_t* out_size);
    static bool WriteFile(const char* path, const uint8_t* buffer, uint32_t size);
    static void FreeFile(uint8_t* buffer, uint32_t size);
    static int ListDirectory(const char* path, DirEntry* out_entries, int max_entries);
};
