#include "ethernet.h"
#include "arp.h"
#include "ip.h"
#include "../drivers/rtl8139.h"
#include "../serial.h"

#define ETHERTYPE_ARP  0x0806
#define ETHERTYPE_IPV4 0x0800

struct EthernetHeader {
    uint8_t dst[6];
    uint8_t src[6];
    uint16_t ethertype; // kolejnosc sieciowa (big-endian) - patrz Swap16 nizej
} __attribute__((packed));

// Pola wieloajtowe w naglowkach sieciowych sa w kolejnosci sieciowej (big-endian), a x86_64
// jest little-endian - zamiana bajtow potrzebna przy KAZDYM odczycie/zapisie takiego pola.
// Funkcja symetryczna (ta sama zamiana w obie strony), stad jedna nazwa bez host/net w nazwie.
static uint16_t Swap16(uint16_t v) {
    return (uint16_t)((v >> 8) | (v << 8));
}

bool Ethernet::Send(const uint8_t dst_mac[6], uint16_t ethertype, const uint8_t* payload, uint16_t payload_len) {
    uint8_t buf[1514];
    if ((uint32_t)payload_len + sizeof(EthernetHeader) > sizeof(buf)) return false;

    EthernetHeader* hdr = (EthernetHeader*)buf;
    for (int i = 0; i < 6; i++) hdr->dst[i] = dst_mac[i];
    RTL8139::GetMacAddress(hdr->src);
    hdr->ethertype = Swap16(ethertype);

    for (uint16_t i = 0; i < payload_len; i++) buf[sizeof(EthernetHeader) + i] = payload[i];

    return RTL8139::Send(buf, (uint16_t)(sizeof(EthernetHeader) + payload_len));
}

void Ethernet::HandleFrame(const uint8_t* frame, uint16_t frame_len) {
    if (frame_len < sizeof(EthernetHeader)) return;

    const EthernetHeader* hdr = (const EthernetHeader*)frame;
    uint16_t ethertype = Swap16(hdr->ethertype);

    const uint8_t* payload = frame + sizeof(EthernetHeader);
    uint16_t payload_len = (uint16_t)(frame_len - sizeof(EthernetHeader));

    if (ethertype == ETHERTYPE_ARP) {
        ARP::HandleFrame(payload, payload_len);
    } else if (ethertype == ETHERTYPE_IPV4) {
        IP::HandleFrame(payload, payload_len);
    }
    // Inne EtherType na razie ciche - surowy naglowek kazdej ramki i tak jest juz logowany
    // przez RTL8139::ReadOnePacket (Faza 2b).
}
