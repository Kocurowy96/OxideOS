#pragma once
#include <stdint.h>

class ATA {
public:
    static void Init();
    static bool ReadSector(uint32_t lba, uint8_t* buffer);
    static bool ReadSectors(uint32_t lba, uint8_t count, uint8_t* buffer);
    static bool WriteSector(uint32_t lba, const uint8_t* buffer);
    static bool WriteSectors(uint32_t lba, uint8_t count, const uint8_t* buffer);

    // Primary slave (drugi dysk na tej samej magistrali/portach co primary master,
    // np. QEMU "-hdb"). Na razie uzywane tylko do testowania sterownika ext2
    // (patrz kernel/fs/ext2.cpp) na osobnym obrazie, bez dotykania disk.img/FAT32.
    static bool ReadSectorSlave(uint32_t lba, uint8_t* buffer);
    static bool ReadSectorsSlave(uint32_t lba, uint8_t count, uint8_t* buffer);
    // Faza 2a: analogicznie dla zapisu - potrzebne zeby przetestowac AllocateBlock/
    // FreeBlock (i pozniej WriteFile) na osobnym obrazie testowym, zanim ext2 w ogole
    // dotknie disk.img (patrz Faza 3b, gdzie sterownik przechodzi na primary master).
    static bool WriteSectorSlave(uint32_t lba, const uint8_t* buffer);
    static bool WriteSectorsSlave(uint32_t lba, uint8_t count, const uint8_t* buffer);

private:
    static void Wait();
    static bool ReadSectorsInternal(uint8_t drive_select_base, uint32_t lba, uint8_t count, uint8_t* buffer);
    static bool WriteSectorsInternal(uint8_t drive_select_base, uint32_t lba, uint8_t count, const uint8_t* buffer);
};
