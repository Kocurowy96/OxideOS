#include "http.h"
#include "tcp.h"
#include "../serial.h"

static int StrLen(const char* s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

// Dopisuje s do bufora, saturating (nie przepelnia) jesli zabraklo miejsca - proste
// zabezpieczenie zamiast bezkrytycznego kopiowania, bez potrzeby pelnego snprintf.
static uint32_t AppendStr(uint8_t* buf, uint32_t pos, uint32_t buf_size, const char* s) {
    int len = StrLen(s);
    for (int i = 0; i < len && pos + (uint32_t)i < buf_size; i++) {
        buf[pos + i] = (uint8_t)s[i];
    }
    uint32_t new_pos = pos + (uint32_t)len;
    return new_pos < buf_size ? new_pos : buf_size;
}

uint32_t HTTP::Get(const uint8_t dst_ip[4], uint16_t dst_port, const char* host,
                    const char* path, uint8_t* out_buf, uint32_t out_buf_len) {
    if (!TCP::Connect(dst_ip, dst_port)) {
        SerialPort::WriteString("HTTP: connection failed.\n");
        return 0;
    }

    uint8_t request[512];
    uint32_t pos = 0;
    pos = AppendStr(request, pos, sizeof(request), "GET ");
    pos = AppendStr(request, pos, sizeof(request), path);
    pos = AppendStr(request, pos, sizeof(request), " HTTP/1.0\r\nHost: ");
    pos = AppendStr(request, pos, sizeof(request), host);
    pos = AppendStr(request, pos, sizeof(request), "\r\nConnection: close\r\n\r\n");

    SerialPort::WriteString("HTTP: sending request...\n");
    if (!TCP::SendData(request, (uint16_t)pos)) {
        SerialPort::WriteString("HTTP: failed to send request.\n");
        TCP::Close();
        return 0;
    }

    // Czytamy az do EOF (TCP::Receive zwraca 0) - dla HTTP/1.0 bez "Connection: keep-alive"
    // to WLASNIE serwer zamyka polaczenie po wyslaniu calej odpowiedzi, wiec EOF = koniec
    // odpowiedzi. Nie parsujemy Content-Length ani nie obslugujemy chunked encoding
    // (swiadomie poza zakresem, patrz http.h).
    uint32_t total = 0;
    while (total < out_buf_len) {
        uint32_t remaining = out_buf_len - total;
        uint16_t want = (uint16_t)(remaining > 1500 ? 1500 : remaining);
        uint16_t chunk = TCP::Receive(out_buf + total, want);
        if (chunk == 0) break;
        total += chunk;
    }

    TCP::Close();
    return total;
}
