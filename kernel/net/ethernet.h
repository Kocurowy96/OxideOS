#pragma once
#include <stdint.h>

// Faza 3 (patrz CoworkWithClaude/PLAN_networking.md): budowanie/parsowanie naglowka
// Ethernet + rozdzielanie ramek do wlasciwej warstwy wyzej (ARP dzis, IP w Fazie 4) po
// EtherType. Swiadome sprzezenie: kernel/drivers/rtl8139.cpp woła Ethernet::HandleFrame
// bezposrednio (nie przez callback/rejestracje protokolow) - prostota > przedwczesna
// abstrakcja, ten sam styl co reszta projektu (np. VFS:: wola FAT32::/Ext2:: wprost).
class Ethernet {
public:
    // Buduje pelna ramke (naglowek Ethernet + payload) i wysyla przez RTL8139::Send.
    static bool Send(const uint8_t dst_mac[6], uint16_t ethertype, const uint8_t* payload, uint16_t payload_len);

    // Parsuje naglowek Ethernet z surowej ramki (od kernel/drivers/rtl8139.cpp) i
    // rozdziela do wlasciwego handlera wg EtherType.
    static void HandleFrame(const uint8_t* frame, uint16_t frame_len);
};
