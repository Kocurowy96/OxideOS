#pragma once
#include <stdint.h>

struct GDTEntry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

struct GDTPointer {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

class GDT {
public:
    static void Init();
private:
    static void SetGate(int num, uint64_t base, uint64_t limit, uint8_t access, uint8_t gran);
};
