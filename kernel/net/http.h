#pragma once
#include <stdint.h>

// Faza 6c (patrz CoworkWithClaude/PLAN_networking.md), TYLKO uruchomiona bo Faza 6b (TCP)
// w pelni dziala i zweryfikowana: minimalny klient HTTP/1.0 zbudowany na TCP:: - jedno
// zadanie GET, bez chunked encoding, bez TLS, bez utrzymywania polaczenia (HTTP/1.0 =
// serwer zamyka polaczenie po jednej odpowiedzi, co pasuje idealnie do jednorazowego,
// blokujacego TCP:: z Fazy 6b).
class HTTP {
public:
    // Laczy sie do dst_ip:dst_port, wysyla "GET <path> HTTP/1.0\r\nHost: <host>\r\n\r\n",
    // odbiera cala odpowiedz (naglowki + cialo, az serwer zamknie polaczenie) do out_buf,
    // zamyka polaczenie. Zwraca faktyczna liczbe odebranych bajtow (0 przy niepowodzeniu
    // polaczenia) - out_buf NIE jest zero-terminowany automatycznie (odpowiedz to dane
    // binarne w ogolnym przypadku), wolajacy dopisuje '\0' sam jesli chce traktowac to
    // jako string.
    static uint32_t Get(const uint8_t dst_ip[4], uint16_t dst_port, const char* host,
                         const char* path, uint8_t* out_buf, uint32_t out_buf_len);
};
