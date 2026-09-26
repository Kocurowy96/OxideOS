#include "ip.h"
#include "arp.h"
#include "ethernet.h"
#include "icmp.h"
#include "udp.h"
#include "tcp.h"
#include "config.h"
#include "../serial.h"

#define ETHERTYPE_IPV4    0x0800
#define IP_PROTOCOL_ICMP  1
#define IP_PROTOCOL_TCP   6
#define IP_PROTOCOL_UDP   17

struct IPv4Header {
    uint8_t version_ihl;   // wersja(4b, gora)=4, IHL(4b, dol)=5 (20B, bez opcji)
    uint8_t tos;
    uint16_t total_length; // kolejnosc sieciowa - wlacznie z tym naglowkiem
    uint16_t id;            // kolejnosc sieciowa
    uint16_t flags_frag;    // kolejnosc sieciowa - zawsze 0 (bez fragmentacji w tej fazie)
    uint8_t ttl;
    uint8_t protocol;       // 1=ICMP, 6=TCP (przyszlosc), 17=UDP (przyszlosc)
    uint16_t checksum;      // kolejnosc sieciowa
    uint8_t src_ip[4];
    uint8_t dst_ip[4];
} __attribute__((packed));

static uint16_t Swap16(uint16_t v) {
    return (uint16_t)((v >> 8) | (v << 8));
}

// Standardowa suma kontrolna internetowa (RFC 1071) - jedynkowe dopelnienie sumy slow
// 16-bitowych, interpretowanych jako big-endian (kolejnosc sieciowa) niezaleznie od tego
// jak poszczegolne pola byly zapisane w pamieci - liczymy na SUROWYCH bajtach bufora
// gotowego do wyslania/wlasnie odebranego, nie na wartosciach pol struktury.
static uint16_t ComputeChecksum(const uint8_t* data, uint16_t len) {
    uint32_t sum = 0;
    uint16_t i = 0;
    for (; (uint16_t)(i + 1) < len; i += 2) {
        sum += ((uint16_t)data[i] << 8) | data[i + 1];
    }
    if (i < len) {
        sum += ((uint16_t)data[i] << 8); // nieparzysta dlugosc - ostatni bajt dopelniony 0
    }
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

static uint16_t next_id = 1;

bool IP::Send(const uint8_t dst_ip[4], uint8_t protocol, const uint8_t* payload, uint16_t payload_len) {
    uint8_t dst_mac[6];
    // Rozglaszanie IP (255.255.255.255) - uzywane przez DHCP (Faza 6a) zanim klient ma
    // jakikolwiek przydzielony adres/brame do ARP-owania. Nie ma sensu pytac ARP-em "kto ma
    // adres rozgloszeniowy" - idzie wprost na rozgloszeniowy adres Ethernet, pomijajac ARP
    // calkowicie. Wszystkie dotychczasowe wywolania (ICMP/UDP) przekazuja realne adresy
    // jednostkowe, wiec ta galaz nigdy ich nie dotyczy.
    bool is_broadcast = dst_ip[0] == 255 && dst_ip[1] == 255 && dst_ip[2] == 255 && dst_ip[3] == 255;
    if (is_broadcast) {
        for (int i = 0; i < 6; i++) dst_mac[i] = 0xFF;
    } else if (!ARP::Resolve(dst_ip, dst_mac)) {
        ARP::SendRequest(dst_ip);
        // Ograniczone czekanie na odpowiedz ARP - w emulacji (SLIRP) zazwyczaj blyskawiczne
        // (patrz Faza 3, ~34us), ale NIE zakladamy tego na sztywno - prawdziwy limit
        // iteracji zamiast wiecznego czekania (ten sam wzorzec co ATA/RTL8139 wczesniej
        // w tej sesji). Brak kolejkowania pakietu na czas rozwiazywania - swiadome
        // uproszczenie, "prawdziwe" stosy IP by to robily.
        uint32_t spins = 0;
        while (!ARP::Resolve(dst_ip, dst_mac)) {
            if (++spins > 5000000) {
                SerialPort::WriteString("IP: ARP resolution timeout.\n");
                return false;
            }
        }
    }

    uint8_t buf[1500];
    uint16_t total_len = (uint16_t)(sizeof(IPv4Header) + payload_len);
    if (total_len > sizeof(buf)) return false;

    IPv4Header* hdr = (IPv4Header*)buf;
    hdr->version_ihl = 0x45;
    hdr->tos = 0;
    hdr->total_length = Swap16(total_len);
    hdr->id = Swap16(next_id++);
    hdr->flags_frag = 0;
    hdr->ttl = 64;
    hdr->protocol = protocol;
    hdr->checksum = 0; // zerowane PRZED liczeniem sumy - wymog algorytmu
    for (int i = 0; i < 4; i++) hdr->src_ip[i] = NET_OUR_IP[i];
    for (int i = 0; i < 4; i++) hdr->dst_ip[i] = dst_ip[i];
    hdr->checksum = Swap16(ComputeChecksum(buf, sizeof(IPv4Header)));

    for (uint16_t i = 0; i < payload_len; i++) buf[sizeof(IPv4Header) + i] = payload[i];

    return Ethernet::Send(dst_mac, ETHERTYPE_IPV4, buf, total_len);
}

void IP::HandleFrame(const uint8_t* data, uint16_t len) {
    if (len < sizeof(IPv4Header)) return;

    const IPv4Header* hdr = (const IPv4Header*)data;
    uint16_t total_len = Swap16(hdr->total_length);
    if (total_len > len || total_len < sizeof(IPv4Header)) return; // nieprawdopodobne/uciete

    const uint8_t* payload = data + sizeof(IPv4Header);
    uint16_t payload_len = (uint16_t)(total_len - sizeof(IPv4Header));

    if (hdr->protocol == IP_PROTOCOL_ICMP) {
        ICMP::HandleFrame(hdr->src_ip, payload, payload_len);
    } else if (hdr->protocol == IP_PROTOCOL_UDP) {
        UDP::HandleFrame(hdr->src_ip, payload, payload_len);
    } else if (hdr->protocol == IP_PROTOCOL_TCP) {
        TCP::HandleFrame(hdr->src_ip, payload, payload_len);
    }
    // Inne protokoly - po cichu ignorowane.
}
