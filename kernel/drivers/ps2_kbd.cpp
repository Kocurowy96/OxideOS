#include "ps2_kbd.h"
#include "../serial.h"
#include "pic.h"
#include "../gui/compositor.h"

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) : "memory");
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

// Simple US QWERTY Scancode Set 1 mapping
static const char scancode_ascii[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-',
    0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void Keyboard::Init() {
    PIC::ClearMask(1); // Unmask IRQ1
    SerialPort::WriteString("PS/2 Keyboard Driver Initialized.\n");
}

char Keyboard::GetAscii(uint8_t scancode) {
    if (scancode < sizeof(scancode_ascii)) {
        return scancode_ascii[scancode];
    }
    return 0;
}

void Keyboard::HandleInterrupt() {
    uint8_t status = inb(0x64);
    if (status & 0x01) {
        uint8_t scancode = inb(0x60);
        if (!(scancode & 0x80)) { // Key press
            char c = GetAscii(scancode);
            if (c) {
                SerialPort::WriteChar(c);
                Compositor::HandleKeyPress(c);
            }
        }
    }
}
