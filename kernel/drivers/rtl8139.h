#pragma once
#include <stdint.h>

// Faza 1a/1b/2a (patrz CoworkWithClaude/PLAN_networking.md): wykrycie karty + odczyt MAC
// (1a), wlaczenie RX/TX + bufor odbiorczy + IRQ (1b), wysylanie surowych ramek (2a).
// Odbior/interpretacja ramek (Ethernet/ARP/IP/ICMP...) to Faza 2b+ - HandleInterrupt() dzis
// tylko potwierdza przerwanie, jeszcze nic nie parsuje z bufora RX.
class RTL8139 {
public:
    static void Init();
    static bool IsPresent();
    static void GetMacAddress(uint8_t out_mac[6]);
    static void HandleInterrupt();
    static uint8_t GetIrqLine();

    // Wysyla surowa ramke Ethernet (naglowek + payload, BEZ FCS - karta sama je dolicza).
    // Synchroniczne - czeka (z limitem) na potwierdzenie transmisji (TOK) zanim wroci.
    // Dopelnia do minimalnej dlugosci ramki Ethernet (60B bez FCS) zerami w razie potrzeby.
    static bool Send(const uint8_t* data, uint16_t len);
};
