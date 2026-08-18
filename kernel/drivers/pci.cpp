#include "pci.h"
#include "../cpu/io.h"
#include "../serial.h"

uint32_t PCI::ConfigRead32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(0xCF8, address);
    return inl(0xCFC);
}

uint16_t PCI::ConfigRead16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t read = ConfigRead32(bus, slot, func, offset);
    return (uint16_t)((read >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t PCI::ConfigRead8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t read = ConfigRead32(bus, slot, func, offset);
    return (uint8_t)((read >> ((offset & 3) * 8)) & 0xFF);
}

void PCI::ConfigWrite32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(0xCF8, address);
    outl(0xCFC, value);
}

void PCI::ConfigWrite16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(0xCF8, address);
    outw(0xCFC + (offset & 2), value);
}

void PCI::ConfigWrite8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t value) {
    uint32_t address = (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));
    outl(0xCF8, address);
    outb(0xCFC + (offset & 3), value);
}

void PCI::Init() {
    SerialPort::WriteString("PCI: Initializing...\n");
}

bool PCI::FindDevice(uint16_t vendor, uint16_t device, uint8_t* out_bus, uint8_t* out_slot, uint8_t* out_func) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint16_t dev_vendor = ConfigRead16(bus, slot, 0, 0);
            if (dev_vendor == 0xFFFF) continue; // No device
            
            for (uint8_t func = 0; func < 8; func++) {
                dev_vendor = ConfigRead16(bus, slot, func, 0);
                if (dev_vendor == 0xFFFF) continue;
                
                uint16_t dev_device = ConfigRead16(bus, slot, func, 2);
                if (dev_vendor == vendor && dev_device == device) {
                    *out_bus = bus;
                    *out_slot = slot;
                    *out_func = func;
                    return true;
                }
            }
        }
    }
    return false;
}
