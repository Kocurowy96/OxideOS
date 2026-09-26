#include "icmp.h"
#include "ip.h"
#include "../serial.h"

#define ICMP_ECHO_REPLY   0
#define ICMP_ECHO_REQUEST 8
#define IP_PROTOCOL_ICMP  1

struct IcmpHeader {
    uint8_t type;
    uint8_t code;
    uint16_t checksum; // kolejnosc sieciowa
    uint16_t id;         // kolejnosc sieciowa
    uint16_t seq;        // kolejnosc sieciowa
} __attribute__((packed));

static uint16_t Swap16(uint16_t v) {
    return (uint16_t)((v >> 8) | (v << 8));
}

// Ten sam algorytm co IP::Send (RFC 1071) - zduplikowany celowo, ten sam styl co reszta
// projektu (kazdy plik ma wlasne male helpery, patrz HOW_WE_WORK.md).
static uint16_t ComputeChecksum(const uint8_t* data, uint16_t len) {
    uint32_t sum = 0;
    uint16_t i = 0;
    for (; (uint16_t)(i + 1) < len; i += 2) {
        sum += ((uint16_t)data[i] << 8) | data[i + 1];
    }
    if (i < len) {
        sum += ((uint16_t)data[i] << 8);
    }
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

void ICMP::SendEchoRequest(const uint8_t dst_ip[4], uint16_t id, uint16_t seq) {
    uint8_t buf[8 + 4]; // naglowek ICMP (8B) + maly payload testowy ("ping", 4B)
    IcmpHeader* hdr = (IcmpHeader*)buf;
    hdr->type = ICMP_ECHO_REQUEST;
    hdr->code = 0;
    hdr->checksum = 0; // zerowane PRZED liczeniem sumy
    hdr->id = Swap16(id);
    hdr->seq = Swap16(seq);
    buf[8] = 'p'; buf[9] = 'i'; buf[10] = 'n'; buf[11] = 'g';
    hdr->checksum = Swap16(ComputeChecksum(buf, sizeof(buf)));

    if (!IP::Send(dst_ip, IP_PROTOCOL_ICMP, buf, sizeof(buf))) {
        SerialPort::WriteString("ICMP: failed to send echo request.\n");
        return;
    }

    SerialPort::WriteString("ICMP: sent echo request to ");
    WriteIp(dst_ip);
    SerialPort::WriteString(" seq=");
    WriteDecimal(seq);
    SerialPort::WriteString("\n");
}

static void SendEchoReply(const uint8_t dst_ip[4], const IcmpHeader* req, const uint8_t* data, uint16_t len) {
    uint8_t buf[128];
    if (len > sizeof(buf)) return; // nieprawdopodobnie duzy ping - pomijamy zamiast przepelniac

    IcmpHeader* hdr = (IcmpHeader*)buf;
    hdr->type = ICMP_ECHO_REPLY;
    hdr->code = 0;
    hdr->checksum = 0;
    hdr->id = req->id;   // juz w kolejnosci sieciowej - kopiujemy wprost, bez zamiany
    hdr->seq = req->seq;
    for (uint16_t i = sizeof(IcmpHeader); i < len; i++) buf[i] = data[i];
    hdr->checksum = Swap16(ComputeChecksum(buf, len));

    IP::Send(dst_ip, IP_PROTOCOL_ICMP, buf, len);
}

void ICMP::HandleFrame(const uint8_t src_ip[4], const uint8_t* data, uint16_t len) {
    if (len < sizeof(IcmpHeader)) return;

    const IcmpHeader* hdr = (const IcmpHeader*)data;

    if (hdr->type == ICMP_ECHO_REPLY) {
        SerialPort::WriteString("ICMP: echo reply from ");
        WriteIp(src_ip);
        SerialPort::WriteString(" seq=");
        WriteDecimal(Swap16(hdr->seq));
        SerialPort::WriteString("\n");
    } else if (hdr->type == ICMP_ECHO_REQUEST) {
        // Odpowiedz na cudzy ping do nas - dla kompletnosci (jak ARP reply w Fazie 3),
        // choc w praktyce nikt nie zapinguje bez hostfwd (patrz notatka w Fazie 2a).
        SendEchoReply(src_ip, hdr, data, len);
    }
}
