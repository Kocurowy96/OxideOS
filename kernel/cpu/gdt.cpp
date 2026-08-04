#include "gdt.h"
#include <stddef.h>

static GDTEntry gdt[7];
static GDTPointer gdt_ptr;
static TSS tss;

extern "C" void gdt_flush(uint64_t);
extern "C" void tss_flush(void);

void GDT::SetGate(int num, uint64_t base, uint64_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;

    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;

    gdt[num].granularity |= gran & 0xF0;
    gdt[num].access = access;
}

void GDT::SetTSSGate(int num, uint64_t base, uint64_t limit, uint8_t access, uint8_t gran) {
    TSSEntry* tss_entry = (TSSEntry*)&gdt[num];
    tss_entry->base_low = (base & 0xFFFF);
    tss_entry->base_middle = (base >> 16) & 0xFF;
    tss_entry->base_high = (base >> 24) & 0xFF;
    tss_entry->base_upper32 = (base >> 32) & 0xFFFFFFFF;
    tss_entry->reserved = 0;

    tss_entry->limit_low = (limit & 0xFFFF);
    tss_entry->granularity = (limit >> 16) & 0x0F;
    tss_entry->granularity |= gran & 0xF0;
    tss_entry->access = access;
}

void GDT::Init() {
    gdt_ptr.limit = (sizeof(GDTEntry) * 7) - 1;
    gdt_ptr.base = (uint64_t)&gdt;

    SetGate(0, 0, 0, 0, 0);                // Null segment
    SetGate(1, 0, 0xFFFFF, 0x9A, 0xA0);    // Kernel Code segment (64-bit)
    SetGate(2, 0, 0xFFFFF, 0x92, 0xA0);    // Kernel Data segment (64-bit)
    
    SetGate(3, 0, 0xFFFFF, 0xFA, 0xA0);    // User Code segment (64-bit)
    SetGate(4, 0, 0xFFFFF, 0xF2, 0xA0);    // User Data segment (64-bit)

    // Init TSS
    for(size_t i = 0; i < sizeof(TSS); i++) {
        ((uint8_t*)&tss)[i] = 0;
    }
    tss.iopb_offset = sizeof(TSS);

    SetTSSGate(5, (uint64_t)&tss, sizeof(TSS) - 1, 0x89, 0x00);

    gdt_flush((uint64_t)&gdt_ptr);
    tss_flush();
}

void GDT::SetTSSStack(void* stack) {
    tss.rsp0 = (uint64_t)stack;
}
