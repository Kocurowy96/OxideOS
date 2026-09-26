#pragma once
#include <stdint.h>

// Faza 1a (patrz CoworkWithClaude/PLAN_networking.md): tylko wykrycie karty + odczyt adresu
// MAC. Brak jeszcze obslugi TX/RX/IRQ - to Faza 1b/2.
class RTL8139 {
public:
    static void Init();
    static bool IsPresent();
    static void GetMacAddress(uint8_t out_mac[6]);
};
