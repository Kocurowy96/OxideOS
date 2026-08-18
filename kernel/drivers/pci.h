#pragma once
#include <stdint.h>

class PCI {
public:
    static void Init();
    static uint32_t ConfigRead32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
    static uint16_t ConfigRead16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
    static uint8_t ConfigRead8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
    
    static void ConfigWrite32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);
    static void ConfigWrite16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value);
    static void ConfigWrite8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t value);
    
    static bool FindDevice(uint16_t vendor, uint16_t device, uint8_t* out_bus, uint8_t* out_slot, uint8_t* out_func);
};
