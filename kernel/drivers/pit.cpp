#include "pit.h"
#include "pic.h"

static uint64_t ticks = 0;

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

void PIT::Init(uint32_t frequency) {
    uint32_t divisor = 1193180 / frequency;

    outb(0x43, 0x36);
    uint8_t l = (uint8_t)(divisor & 0xFF);
    uint8_t h = (uint8_t)((divisor >> 8) & 0xFF);

    outb(0x40, l);
    outb(0x40, h);
    
    PIC::ClearMask(0); // Unmask IRQ0
}

void PIT::Tick() {
    ticks++;
}

uint64_t PIT::GetTicks() {
    return ticks;
}
