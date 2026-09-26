#pragma once
#include <stdint.h>

// Faza 4 (KAMIEN MILOWY, patrz CoworkWithClaude/PLAN_networking.md): ICMP echo request/reply.
class ICMP {
public:
    static void SendEchoRequest(const uint8_t dst_ip[4], uint16_t id, uint16_t seq);

    // src_ip: nadawca z naglowka IP (od IP::HandleFrame) - potrzebny zeby odpowiedziec na
    // cudzy echo request (nasz wlasny adres jest w NET_OUR_IP, znany globalnie).
    static void HandleFrame(const uint8_t src_ip[4], const uint8_t* data, uint16_t len);
};
