#include "vfs.h"
#include "ext2.h"
#include "../serial.h"

// Faza 3b (patrz CoworkWithClaude/PLAN_ext2_filesystem.md): VFS:: wola teraz Ext2::
// zamiast FAT32:: - disk.img od Fazy 3a jest juz obrazem ext2 (mke2fs), nie FAT32.
// Uwaga na sciezki: FAT32:: (fat32.cpp) NIE tolerowalo wiodacego '/' (jego prosty
// parser FindDirectoryCluster/ResolveDirectoryCluster zaklada sciezke bez niego),
// stad ponizsze funkcje kiedys go obcinaly przed wywolaniem FAT32::. Ext2:: jest
// odwrotnie - ResolvePath/ListDirectory/ReadFile/WriteFile byly projektowane i
// testowane od Fazy 1c z wiodacym '/' (np. test "/lost+found"), a WriteFile->
// SplitParentAndName (Faza 2c) go WYMAGA (zwraca false bez niego). Dlatego sciezka
// NIE jest juz obcinana - wszyscy wywolujacy VFS:: w calym kernelu i tak zawsze
// przekazuja sciezki bezwzgledne (potwierdzone: kernel/main.cpp, compositor.cpp,
// syscall.cpp, wszystkie apki uzywajace sys_write_file/sys_read_file). FAT32::
// zostaje nietkniete w drzewie (fat32.cpp), na wypadek gdyby trzeba je bylo
// jeszcze kiedys zweryfikowac/porownac, ale VFS:: juz go nie wywoluje.
void VFS::Init() {
    SerialPort::WriteString("VFS: Initializing Virtual File System...\n");
    Ext2::Init();
}

bool VFS::ReadFile(const char* path, uint8_t** out_buffer, uint32_t* out_size) {
    return Ext2::ReadFile(path, out_buffer, out_size);
}

bool VFS::WriteFile(const char* path, const uint8_t* buffer, uint32_t size) {
    return Ext2::WriteFile(path, buffer, size);
}

void VFS::FreeFile(uint8_t* buffer, uint32_t size) {
    Ext2::FreeFile(buffer, size);
}

int VFS::ListDirectory(const char* path, DirEntry* out_entries, int max_entries) {
    return Ext2::ListDirectory(path, out_entries, max_entries);
}
