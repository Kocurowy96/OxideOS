#include "idt.h"

static IDTEntry idt[256];
static IDTPointer idt_ptr;

extern "C" void idt_flush(uint64_t);
extern "C" uint64_t isr_stub_table[];

void IDT::SetGate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low = (base & 0xFFFF);
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].base_highest = (base >> 32) & 0xFFFFFFFF;

    idt[num].sel = sel;
    idt[num].always0 = 0;
    idt[num].flags = flags;
    idt[num].reserved = 0;
}

void IDT::Init() {
    idt_ptr.limit = sizeof(IDTEntry) * 256 - 1;
    idt_ptr.base = (uint64_t)&idt;

    for (int i = 0; i < 256; i++) {
        SetGate(i, 0, 0, 0);
    }

    for (int i = 0; i < 256; i++) {
        if (i == 0x80) {
            SetGate(i, isr_stub_table[i], 0x08, 0xEE); // 0xEE: Interrupt Gate, Ring 3
        } else {
            SetGate(i, isr_stub_table[i], 0x08, 0x8E); // 0x8E: Interrupt Gate, Ring 0
        }
    }

    idt_flush((uint64_t)&idt_ptr);
}
