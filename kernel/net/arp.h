#pragma once
#include <stdint.h>

// Faza 3 (patrz CoworkWithClaude/PLAN_networking.md): ARP - odwzorowanie IP<->MAC.
class ARP {
public:
    // Wysyla ARP request (broadcast) pytajacy o MAC dla podanego IP.
    static void SendRequest(const uint8_t target_ip[4]);

    // Parsuje pakiet ARP (payload ramki Ethernet, bez naglowka Ethernet) - zapamietuje
    // nadawce w lokalnej tablicy, odpowiada jesli to request o nasz wlasny adres.
    static void HandleFrame(const uint8_t* data, uint16_t len);

    // Szuka IP w lokalnej tablicy ARP. Zwraca false jesli nieznane (trzeba wpierw
    // SendRequest i poczekac na odpowiedz - Faza 3 nie ma jeszcze blokujacego czekania,
    // to zostawione warstwie wyzej/Fazie 4 do zdecydowania jak sobie z tym poradzic).
    static bool Resolve(const uint8_t ip[4], uint8_t out_mac[6]);
};
