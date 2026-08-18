#pragma once
#include <stdint.h>

class Keyboard {
public:
    static void Init();
    static void HandleInterrupt();
    static char GetAscii(uint8_t scancode);
};
