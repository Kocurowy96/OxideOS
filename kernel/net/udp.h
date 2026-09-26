#pragma once
#include <stdint.h>

// Faza 5 (patrz CoworkWithClaude/PLAN_networking.md): naglowek UDP + prosty serwer echo na
// stalym porcie (RFC 862 "Echo Protocol", port 7 - wybrany celowo, dokladnie to implementujemy).
// Weryfikacja w druga strone niz ICMP (host -> OxideOS) przez `-netdev
// user,...,hostfwd=udp::PORT-:7` + `nc -u localhost <PORT>` z hosta, bo gole UDP "na zewnatrz"
// nie ma powszechnie dostepnej uslugi-echo jak brama ICMP.
class UDP {
public:
    static bool Send(const uint8_t dst_ip[4], uint16_t src_port, uint16_t dst_port,
                      const uint8_t* payload, uint16_t payload_len);

    // src_ip: nadawca z naglowka IP (jak ICMP::HandleFrame) - potrzebny zeby odeslac echo.
    static void HandleFrame(const uint8_t src_ip[4], const uint8_t* data, uint16_t len);
};
