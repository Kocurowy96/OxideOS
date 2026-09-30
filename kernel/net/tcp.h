#pragma once
#include <stdint.h>

// Faza 6b (patrz CoworkWithClaude/PLAN_networking.md): TCP - swiadomie waski zakres,
// ustalony z gory (nie do rozszerzania w tej fazie): jeden klient, JEDNO polaczenie na
// raz, API blokujace, brak roli serwera (bez nasluchiwania/akceptowania), brak pelnej
// kontroli przeciazenia/okna, brak retransmisji (SLIRP to "czyste" wirtualne lacze bez
// realnej utraty pakietow - swiadomie zaakceptowane ryzyko, jak dla calego zakresu tej
// fazy). Wystarcza: handshake (SYN/SYN-ACK/ACK), wysylanie/odbieranie danych, FIN
// zamykajacy polaczenie.
class TCP {
public:
    // Otwiera polaczenie (blokujace, ograniczone czekanie na handshake). Tylko jedno
    // polaczenie na raz - kolejne wywolanie przed Close() poprzedniego zwraca false.
    static bool Connect(const uint8_t dst_ip[4], uint16_t dst_port);

    // Wysyla dane na juz otwartym polaczeniu - blokuje do potwierdzenia (ACK) calego
    // segmentu, bez potokowania wielu segmentow na raz.
    static bool SendData(const uint8_t* data, uint16_t len);

    // Odbiera dane (blokujace, z ograniczonym czekaniem) - zwraca faktyczna liczbe
    // odebranych bajtow (moze byc mniejsza niz max_len, jak read()), 0 jesli polaczenie
    // zamkniete przez drugą stronę (FIN) i bufor pusty, albo timeout.
    static uint16_t Receive(uint8_t* buf, uint16_t max_len);

    // Zamyka polaczenie (wysyla FIN, czeka na potwierdzenie i/lub FIN drugiej strony -
    // ograniczone czekanie, nie wisi w nieskonczonosc jesli druga strona nie odpowie).
    static void Close();

    // Wolane przez IP::HandleFrame dla protokolu 6 (TCP).
    static void HandleFrame(const uint8_t src_ip[4], const uint8_t* data, uint16_t len);
};
