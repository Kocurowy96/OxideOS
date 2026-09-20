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

// Ten sam uklad co scancode_ascii, ale ze znakami po Shift - 0 tam gdzie Shift nic nie
// zmienia (litery obslugiwane osobno w GetAscii przez caps_lock XOR shift_held).
static const char scancode_ascii_shifted[] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-',
    0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

#define KEY_LSHIFT 0x2A
#define KEY_RSHIFT 0x36
#define KEY_CAPSLOCK 0x3A

static bool shift_held = false;
static bool caps_lock = false;

void Keyboard::Init() {
    PIC::ClearMask(1); // Unmask IRQ1
    SerialPort::WriteString("PS/2 Keyboard Driver Initialized.\n");
}

char Keyboard::GetAscii(uint8_t scancode) {
    if (scancode >= sizeof(scancode_ascii)) return 0;
    char base = scancode_ascii[scancode];
    if (base >= 'a' && base <= 'z') {
        bool uppercase = shift_held != caps_lock; // XOR - albo Shift, albo Caps, nie oba naraz
        return uppercase ? scancode_ascii_shifted[scancode] : base;
    }
    if (shift_held) {
        char shifted = scancode_ascii_shifted[scancode];
        if (shifted) return shifted;
    }
    return base;
}

void Keyboard::HandleInterrupt() {
    uint8_t status = inb(0x64);
    if (status & 0x01) {
        uint8_t raw = inb(0x60);
        bool released = raw & 0x80;
        uint8_t scancode = raw & 0x7F;

        if (scancode == KEY_LSHIFT || scancode == KEY_RSHIFT) {
            shift_held = !released;
            return;
        }
        if (scancode == KEY_CAPSLOCK) {
            if (!released) caps_lock = !caps_lock; // przelacz tylko na key-down
            return;
        }

        if (!released) {
            char c = GetAscii(scancode);
            if (c) {
                SerialPort::WriteChar(c);
                Compositor::HandleKeyPress(c);
            }
        }
    }
}
