#pragma once
#include <stdint.h>

// Faza 1a/1b (patrz CoworkWithClaude/PLAN_networking.md): wykrycie karty + odczyt MAC (1a),
// wlaczenie RX/TX + bufor odbiorczy + IRQ (1b). Interpretacja ramek (Ethernet/ARP/IP/ICMP...)
// to Faza 2+ - HandleInterrupt() dzis tylko potwierdza przerwanie, jeszcze nic nie parsuje.
class RTL8139 {
public:
    static void Init();
    static bool IsPresent();
    static void GetMacAddress(uint8_t out_mac[6]);
    static void HandleInterrupt();
    static uint8_t GetIrqLine();
};
