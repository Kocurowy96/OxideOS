#include "arp.h"
#include "ethernet.h"
#include "config.h"
#include "../drivers/rtl8139.h"
#include "../serial.h"

#define ARP_HTYPE_ETHERNET 1
#define ARP_PTYPE_IPV4     0x0800
#define ARP_OP_REQUEST     1
#define ARP_OP_REPLY       2
#define ETHERTYPE_ARP      0x0806

struct ArpPacket {
    uint16_t htype; // kolejnosc sieciowa
    uint16_t ptype; // kolejnosc sieciowa
    uint8_t hlen;
    uint8_t plen;
    uint16_t oper;  // kolejnosc sieciowa
    uint8_t sha[6]; // sender hardware address (MAC)
    uint8_t spa[4]; // sender protocol address (IP)
    uint8_t tha[6]; // target hardware address (MAC)
    uint8_t tpa[4]; // target protocol address (IP)
} __attribute__((packed));

static uint16_t Swap16(uint16_t v) {
    return (uint16_t)((v >> 8) | (v << 8));
}

#define ARP_TABLE_SIZE 8
struct ArpEntry {
    uint8_t ip[4];
    uint8_t mac[6];
    bool valid;
};
static ArpEntry arp_table[ARP_TABLE_SIZE] = {};

static void WriteHexByte(uint8_t val) {
    static const char hex[] = "0123456789ABCDEF";
    SerialPort::WriteChar(hex[(val >> 4) & 0xF]);
    SerialPort::WriteChar(hex[val & 0xF]);
}

static void WriteIp(const uint8_t ip[4]) {
    for (int i = 0; i < 4; i++) {
        uint8_t v = ip[i];
        char tmp[4];
        int tlen = 0;
        if (v == 0) tmp[tlen++] = '0';
        while (v > 0) { tmp[tlen++] = (char)('0' + (v % 10)); v /= 10; }
        while (tlen > 0) SerialPort::WriteChar(tmp[--tlen]);
        if (i < 3) SerialPort::WriteChar('.');
    }
}

static bool IpEquals(const uint8_t a[4], const uint8_t b[4]) {
    for (int i = 0; i < 4; i++) if (a[i] != b[i]) return false;
    return true;
}

// Zapamietuje/aktualizuje wpis nadawcy - robimy to dla KAZDEGO odebranego pakietu ARP
// (request czy reply), nie tylko odpowiedzi na nasze wlasne zapytania - dokladnie tak jak
// prawdziwe stosy IP ucza sie "przy okazji" z ruchu ktory i tak widza (typowe "gratuitous"
// uczenie cache ARP).
static void LearnEntry(const uint8_t ip[4], const uint8_t mac[6]) {
    int free_slot = -1;
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (arp_table[i].valid && IpEquals(arp_table[i].ip, ip)) {
            for (int j = 0; j < 6; j++) arp_table[i].mac[j] = mac[j];
            return;
        }
        if (!arp_table[i].valid && free_slot < 0) free_slot = i;
    }
    if (free_slot < 0) free_slot = 0; // tablica pelna - nadpisz najstarszy wpis (slot 0)

    for (int i = 0; i < 4; i++) arp_table[free_slot].ip[i] = ip[i];
    for (int i = 0; i < 6; i++) arp_table[free_slot].mac[i] = mac[i];
    arp_table[free_slot].valid = true;

    SerialPort::WriteString("ARP: learned ");
    WriteIp(ip);
    SerialPort::WriteString(" -> ");
    for (int i = 0; i < 6; i++) { WriteHexByte(mac[i]); if (i < 5) SerialPort::WriteChar(':'); }
    SerialPort::WriteString("\n");
}

bool ARP::Resolve(const uint8_t ip[4], uint8_t out_mac[6]) {
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (arp_table[i].valid && IpEquals(arp_table[i].ip, ip)) {
            for (int j = 0; j < 6; j++) out_mac[j] = arp_table[i].mac[j];
            return true;
        }
    }
    return false;
}

void ARP::SendRequest(const uint8_t target_ip[4]) {
    ArpPacket pkt;
    pkt.htype = Swap16(ARP_HTYPE_ETHERNET);
    pkt.ptype = Swap16(ARP_PTYPE_IPV4);
    pkt.hlen = 6;
    pkt.plen = 4;
    pkt.oper = Swap16(ARP_OP_REQUEST);
    RTL8139::GetMacAddress(pkt.sha);
    for (int i = 0; i < 4; i++) pkt.spa[i] = NET_OUR_IP[i];
    for (int i = 0; i < 6; i++) pkt.tha[i] = 0; // nieznany - to wlasnie o to pytamy
    for (int i = 0; i < 4; i++) pkt.tpa[i] = target_ip[i];

    static const uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    Ethernet::Send(broadcast_mac, ETHERTYPE_ARP, (const uint8_t*)&pkt, sizeof(pkt));

    SerialPort::WriteString("ARP: sent request for ");
    WriteIp(target_ip);
    SerialPort::WriteString("\n");
}

static void SendReply(const uint8_t target_ip[4], const uint8_t target_mac[6]) {
    ArpPacket pkt;
    pkt.htype = Swap16(ARP_HTYPE_ETHERNET);
    pkt.ptype = Swap16(ARP_PTYPE_IPV4);
    pkt.hlen = 6;
    pkt.plen = 4;
    pkt.oper = Swap16(ARP_OP_REPLY);
    RTL8139::GetMacAddress(pkt.sha);
    for (int i = 0; i < 4; i++) pkt.spa[i] = NET_OUR_IP[i];
    for (int i = 0; i < 6; i++) pkt.tha[i] = target_mac[i];
    for (int i = 0; i < 4; i++) pkt.tpa[i] = target_ip[i];

    Ethernet::Send(target_mac, ETHERTYPE_ARP, (const uint8_t*)&pkt, sizeof(pkt));

    SerialPort::WriteString("ARP: sent reply to ");
    WriteIp(target_ip);
    SerialPort::WriteString("\n");
}

void ARP::HandleFrame(const uint8_t* data, uint16_t len) {
    if (len < sizeof(ArpPacket)) return;

    const ArpPacket* pkt = (const ArpPacket*)data;
    uint16_t oper = Swap16(pkt->oper);

    LearnEntry(pkt->spa, pkt->sha);

    if (oper == ARP_OP_REQUEST && IpEquals(pkt->tpa, NET_OUR_IP)) {
        SendReply(pkt->spa, pkt->sha);
    }
}
