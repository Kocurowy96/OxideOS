#pragma once
#include <stdint.h>

class ATA {
public:
    static void Init();
    static bool ReadSector(uint32_t lba, uint8_t* buffer);
    static bool ReadSectors(uint32_t lba, uint8_t count, uint8_t* buffer);
    static bool WriteSector(uint32_t lba, const uint8_t* buffer);
    static bool WriteSectors(uint32_t lba, uint8_t count, const uint8_t* buffer);
    
private:
    static void Wait();
};
