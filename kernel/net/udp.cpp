#include "udp.h"
#include "ip.h"
#include "config.h"
#include "../serial.h"

#define IP_PROTOCOL_UDP  17
#define UDP_ECHO_PORT    7 // RFC 862 "Echo Protocol" - dokladnie to implementujemy

struct UdpHeader {
    uint16_t src_port; // kolejnosc sieciowa
    uint16_t dst_port; // kolejnosc sieciowa
    uint16_t length;   // kolejnosc sieciowa - naglowek UDP (8B) + dane
    uint16_t checksum; // kolejnosc sieciowa
} __attribute__((packed));

static uint16_t Swap16(uint16_t v) {
    return (uint16_t)((v >> 8) | (v << 8));
}

// Ten sam algorytm RFC 1071 co IP::Send/ICMP (zduplikowany celowo, jak zawsze w tym
// projekcie - patrz HOW_WE_WORK.md), ale rozbity na akumulacje+domkniecie zamiast jednego
// gotowego bufora: suma kontrolna UDP musi objac pseudo-naglowek IP (RFC 768 - src/dst IP,
// protokol, dlugosc) ORAZ sam segment UDP, a te dwa nigdy nie sa fizycznie sasiadujacym
// buforem gotowym do wyslania (pseudo-naglowek nie jest czescia ramki) - sklejanie ich w
// jeden wiekszy bufor tylko po to zeby policzyc sume kosztowaloby dodatkowe ~1.5KB stosu
// bez potrzeby.
static void AccumulateSum(const uint8_t* data, uint16_t len, uint32_t* sum) {
    uint16_t i = 0;
    for (; (uint16_t)(i + 1) < len; i += 2) {
        *sum += ((uint16_t)data[i] << 8) | data[i + 1];
    }
    if (i < len) {
        *sum += ((uint16_t)data[i] << 8);
    }
}

static uint16_t FinishChecksum(uint32_t sum) {
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

static void WriteDecimal(uint32_t val) {
    char buf[12];
    int i = 0;
    if (val == 0) buf[i++] = '0';
    while (val > 0) { buf[i++] = '0' + (val % 10); val /= 10; }
    while (i > 0) SerialPort::WriteChar(buf[--i]);
}

static void WriteIp(const uint8_t ip[4]) {
    for (int i = 0; i < 4; i++) {
        WriteDecimal(ip[i]);
        if (i < 3) SerialPort::WriteChar('.');
    }
}

bool UDP::Send(const uint8_t dst_ip[4], uint16_t src_port, uint16_t dst_port,
               const uint8_t* payload, uint16_t payload_len) {
    uint8_t seg[1500];
    uint16_t udp_len = (uint16_t)(sizeof(UdpHeader) + payload_len);
    if (udp_len > sizeof(seg)) return false;

    UdpHeader* hdr = (UdpHeader*)seg;
    hdr->src_port = Swap16(src_port);
    hdr->dst_port = Swap16(dst_port);
    hdr->length = Swap16(udp_len);
    hdr->checksum = 0; // zerowane PRZED liczeniem sumy - wymog algorytmu
    for (uint16_t i = 0; i < payload_len; i++) seg[sizeof(UdpHeader) + i] = payload[i];

    // Pseudo-naglowek RFC 768: src IP (4B) + dst IP (4B) + zero (1B) + protokol (1B) +
    // dlugosc UDP (2B) = 12B.
    uint8_t pseudo[12];
    for (int i = 0; i < 4; i++) pseudo[i] = NET_OUR_IP[i];
    for (int i = 0; i < 4; i++) pseudo[4 + i] = dst_ip[i];
    pseudo[8] = 0;
    pseudo[9] = IP_PROTOCOL_UDP;
    pseudo[10] = (uint8_t)(udp_len >> 8);
    pseudo[11] = (uint8_t)(udp_len & 0xFF);

    uint32_t sum = 0;
    AccumulateSum(pseudo, sizeof(pseudo), &sum);
    AccumulateSum(seg, udp_len, &sum);
    uint16_t checksum = FinishChecksum(sum);
    // 0 w polu sumy kontrolnej UDP oznacza "brak sumy kontrolnej" (dozwolone przez RFC 768,
    // ale niejednoznaczne) - w tym rzadkim przypadku prawdziwej sumy 0 wysylamy 0xFFFF
    // (rownowazne pod jedynkowym dopelnieniem), standardowa konwencja stosow TCP/IP.
    hdr->checksum = Swap16(checksum == 0 ? 0xFFFF : checksum);

    return IP::Send(dst_ip, IP_PROTOCOL_UDP, seg, udp_len);
}

void UDP::HandleFrame(const uint8_t src_ip[4], const uint8_t* data, uint16_t len) {
    if (len < sizeof(UdpHeader)) return;

    const UdpHeader* hdr = (const UdpHeader*)data;
    uint16_t dst_port = Swap16(hdr->dst_port);
    uint16_t src_port = Swap16(hdr->src_port);
    uint16_t udp_len = Swap16(hdr->length);
    if (udp_len > len || udp_len < sizeof(UdpHeader)) return; // nieprawdopodobne/uciete

    const uint8_t* payload = data + sizeof(UdpHeader);
    uint16_t payload_len = (uint16_t)(udp_len - sizeof(UdpHeader));

    if (dst_port == UDP_ECHO_PORT) {
        SerialPort::WriteString("UDP: echo request od ");
        WriteIp(src_ip);
        SerialPort::WriteString(":");
        WriteDecimal(src_port);
        SerialPort::WriteString(", ");
        WriteDecimal(payload_len);
        SerialPort::WriteString("B - odsylam.\n");

        UDP::Send(src_ip, UDP_ECHO_PORT, src_port, payload, payload_len);
    }
    // Inne porty - po cichu ignorowane (brak zarejestrowanych "gniazd"/callbackow na porty,
    // to dopiero Faza 7 - API dla aplikacji).
}
