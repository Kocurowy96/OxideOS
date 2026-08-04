#pragma once
#include <stdint.h>

struct IDTEntry {
    uint16_t base_low;
    uint16_t sel;
    uint8_t always0;
    uint8_t flags;
    uint16_t base_high;
    uint32_t base_highest;
    uint32_t reserved;
} __attribute__((packed));

struct IDTPointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

class IDT {
public:
    static void Init();
    static void SetGate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags);
};
