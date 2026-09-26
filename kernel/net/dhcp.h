#pragma once
#include <stdint.h>

// Faza 6a (patrz CoworkWithClaude/PLAN_networking.md): klient DHCP - DISCOVER->OFFER->
// REQUEST->ACK, wypelnia NET_OUR_IP/NET_GATEWAY_IP (kernel/net/config.h) prawdziwymi
// wartosciami z serwera zamiast dawnych zaszytych na sztywno stalych. Swiadome
// uproszczenie (jak critical.h): klient dziala RAZ przy starcie kernela, nie odnawia
// dzierzawy (brak sledzenia czasu jej trwania) - jesli kiedys okaze sie to potrzebne
// (dlugo dzialajacy system, realny sprzet z krotkim leasem), do rozszerzenia osobno.
class DHCP {
public:
    // Blokujace (ograniczone czekanie na kazdym kroku, jak ARP::Resolve) - zwraca false
    // jesli serwer nie odpowiedzial w rozsadnym czasie albo odeslal NAK; NET_OUR_IP
    // zostaje wtedy {0,0,0,0}.
    static bool Run();

    // Wolane przez UDP::HandleFrame dla pakietow na porcie klienta DHCP (68).
    static void HandleFrame(const uint8_t* data, uint16_t len);
};
