#include "pic.h"

#define PIC1_COMMAND 0x20
#define PIC1_DATA 0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA 0xA1

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) : "memory");
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

void PIC::Init() {
    outb(PIC1_COMMAND, 0x11);
    io_wait();
    outb(PIC2_COMMAND, 0x11);
    io_wait();

    outb(PIC1_DATA, 0x20); // IRQ 0-7 mapped to ints 32-39
    io_wait();
    outb(PIC2_DATA, 0x28); // IRQ 8-15 mapped to ints 40-47
    io_wait();

    outb(PIC1_DATA, 4); // Tell Master there is a slave PIC at IRQ2
    io_wait();
    outb(PIC2_DATA, 2); // Tell Slave PIC its cascade identity
    io_wait();

    outb(PIC1_DATA, 0x01); // 8086/88 mode
    io_wait();
    outb(PIC2_DATA, 0x01); // 8086/88 mode
    io_wait();

    outb(PIC1_DATA, 0xFB); // mask all except IRQ2
    outb(PIC2_DATA, 0xFF); // mask all
}

void PIC::SendEOI(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, 0x20);
    }
    outb(PIC1_COMMAND, 0x20);
}
