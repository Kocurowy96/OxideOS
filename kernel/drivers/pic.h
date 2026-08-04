#pragma once
#include <stdint.h>

class PIC {
public:
    static void Init();
    static void SendEOI(uint8_t irq);
    static void SetMask(uint8_t irq);
    static void ClearMask(uint8_t irq);
};
