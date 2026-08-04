#include "gdt.h"

static GDTEntry gdt[5];
static GDTPointer gdt_ptr;

extern "C" void gdt_flush(uint64_t);

void GDT::SetGate(int num, uint64_t base, uint64_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;

    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;

    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access = access;
}

void GDT::Init() {
    gdt_ptr.limit = (sizeof(GDTEntry) * 5) - 1;
    gdt_ptr.base = (uint64_t)&gdt;

    SetGate(0, 0, 0, 0, 0);                // Null segment
    SetGate(1, 0, 0xFFFFF, 0x9A, 0xA0);    // Kernel Code segment (64-bit)
    SetGate(2, 0, 0xFFFFF, 0x92, 0xA0);    // Kernel Data segment (64-bit)
    SetGate(3, 0, 0xFFFFF, 0xFA, 0xA0);    // User Code segment (64-bit)
    SetGate(4, 0, 0xFFFFF, 0xF2, 0xA0);    // User Data segment (64-bit)

    gdt_flush((uint64_t)&gdt_ptr);
}
