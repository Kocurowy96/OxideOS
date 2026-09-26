#pragma once
#include <stdint.h>

// Faza 4 (patrz CoworkWithClaude/PLAN_networking.md): IPv4, bez fragmentacji, z suma
// kontrolna. Rozwiazywanie MAC celu przez ARP:: (blokujace, z ograniczonym czekaniem -
// patrz Send() w ip.cpp) - swiadome uproszczenie, brak kolejkowania pakietow w oczekiwaniu
// na ARP jak w "prawdziwych" stosach.
class IP {
public:
    // protocol: 1=ICMP, 6=TCP, 17=UDP (przyszle fazy).
    static bool Send(const uint8_t dst_ip[4], uint8_t protocol, const uint8_t* payload, uint16_t payload_len);

    // Parsuje naglowek IP (payload ramki Ethernet po odjeciu jej wlasnego naglowka) i
    // rozdziela do wlasciwego protokolu wyzej (ICMP dzis, UDP/TCP w przyszlych fazach).
    static void HandleFrame(const uint8_t* data, uint16_t len);
};
