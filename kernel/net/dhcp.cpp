#include "dhcp.h"
#include "udp.h"
#include "config.h"
#include "../drivers/rtl8139.h"
#include "../serial.h"

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68
#define DHCP_MAGIC_COOKIE 0x63825363u

#define DHCP_OP_BOOTREQUEST 1
#define DHCP_OP_BOOTREPLY   2
#define DHCP_HTYPE_ETHERNET 1

#define DHCP_MSG_DISCOVER 1
#define DHCP_MSG_OFFER    2
#define DHCP_MSG_REQUEST  3
#define DHCP_MSG_ACK      5
#define DHCP_MSG_NAK      6

#define OPT_PAD             0
#define OPT_ROUTER          3
#define OPT_REQUESTED_IP    50
#define OPT_MESSAGE_TYPE    53
#define OPT_SERVER_ID       54
#define OPT_PARAM_REQ_LIST  55
#define OPT_END             255

struct DhcpMessage {
    uint8_t op;
    uint8_t htype;
    uint8_t hlen;
    uint8_t hops;
    uint32_t xid;       // kolejnosc sieciowa
    uint16_t secs;      // kolejnosc sieciowa
    uint16_t flags;     // kolejnosc sieciowa
    uint8_t ciaddr[4];
    uint8_t yiaddr[4];
    uint8_t siaddr[4];
    uint8_t giaddr[4];
    uint8_t chaddr[16]; // pierwsze 6B = MAC klienta, reszta dopelnienie zerami
    uint8_t sname[64];
    uint8_t file[128];
    uint32_t magic_cookie; // kolejnosc sieciowa, 0x63825363
    uint8_t options[312];  // wiecej niz potrzebujemy wyslac/odebrac - naglowek + typowa
                           // odpowiedz SLIRP mieszcza sie z duzym zapasem w MTU
} __attribute__((packed));

static uint16_t Swap16(uint16_t v) {
    return (uint16_t)((v >> 8) | (v << 8));
}

static uint32_t Swap32(uint32_t v) {
    return ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) |
           ((v & 0x00FF0000u) >> 8) | ((v & 0xFF000000u) >> 24);
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

// Stan wypelniany przez DHCP::HandleFrame (wolane z lancucha przerwania RX), odpytywany w
// petli przez DHCP::Run() (wolane raz z kontekstu startowego kernela) - ten sam wzorzec co
// blokujace ARP::Resolve odpytujace tablice wypelniana przez ARP::HandleFrame. `volatile`
// z tego samego powodu co g_critical_depth w critical.h - zapis z przerwania, odczyt w
// petli spinowej, kompilator nie moze zalozyc ze sie nie zmieni.
enum class DhcpWaiting { NONE, OFFER, ACK };
static volatile DhcpWaiting g_waiting = DhcpWaiting::NONE;
static volatile bool g_got_nak = false;
static uint32_t g_xid = 0;
static uint8_t g_offered_ip[4] = {0, 0, 0, 0};
static uint8_t g_server_id[4] = {0, 0, 0, 0};
static uint8_t g_router[4] = {0, 0, 0, 0};
static bool g_have_router = false;

static const uint8_t* FindOption(const uint8_t* options, uint16_t options_len, uint8_t tag, uint8_t* out_len) {
    uint16_t i = 0;
    while (i < options_len) {
        uint8_t t = options[i];
        if (t == OPT_END) break;
        if (t == OPT_PAD) { i++; continue; }
        if ((uint16_t)(i + 1) >= options_len) break;
        uint8_t len = options[i + 1];
        if ((uint16_t)(i + 2 + len) > options_len) break;
        if (t == tag) {
            if (out_len) *out_len = len;
            return &options[i + 2];
        }
        i = (uint16_t)(i + 2 + len);
    }
    return nullptr;
}

// requested_ip/server_id: NULL dla DISCOVER, obie podane dla REQUEST (echo tego co
// przyszlo w OFFER, jak wymaga RFC 2131). Zwraca dlugosc faktycznie uzytej czesci bufora
// (staly naglowek + tylko zapisane opcje, nie caly sizeof(DhcpMessage)).
static uint16_t BuildMessage(uint8_t* buf, uint8_t msg_type, const uint8_t* requested_ip, const uint8_t* server_id) {
    DhcpMessage* msg = (DhcpMessage*)buf;
    for (uint16_t i = 0; i < sizeof(DhcpMessage); i++) buf[i] = 0;

    msg->op = DHCP_OP_BOOTREQUEST;
    msg->htype = DHCP_HTYPE_ETHERNET;
    msg->hlen = 6;
    msg->hops = 0;
    msg->xid = Swap32(g_xid);
    msg->secs = 0;
    msg->flags = Swap16(0x8000); // bit rozgloszeniowy - nie mamy jeszcze skonfigurowanego IP
    RTL8139::GetMacAddress(msg->chaddr);
    msg->magic_cookie = Swap32(DHCP_MAGIC_COOKIE);

    uint16_t i = 0;
    msg->options[i++] = OPT_MESSAGE_TYPE;
    msg->options[i++] = 1;
    msg->options[i++] = msg_type;

    if (requested_ip) {
        msg->options[i++] = OPT_REQUESTED_IP;
        msg->options[i++] = 4;
        for (int j = 0; j < 4; j++) msg->options[i++] = requested_ip[j];
    }
    if (server_id) {
        msg->options[i++] = OPT_SERVER_ID;
        msg->options[i++] = 4;
        for (int j = 0; j < 4; j++) msg->options[i++] = server_id[j];
    }

    msg->options[i++] = OPT_PARAM_REQ_LIST;
    msg->options[i++] = 1;
    msg->options[i++] = OPT_ROUTER;

    msg->options[i++] = OPT_END;

    uint16_t fixed_size = (uint16_t)((uint8_t*)msg->options - (uint8_t*)msg);
    return (uint16_t)(fixed_size + i);
}

bool DHCP::Run() {
    // xid stala, nie losowa - ten klient uruchamia sie co najwyzej raz na sesje kernela
    // (patrz uwaga o braku odnawiania dzierzawy), wiec nie ma czego rozrozniac miedzy
    // rownoleglymi probami jak zrobilby "prawdziwy" klient dzialajacy wielokrotnie.
    g_xid = 0x39A17B2Cu;
    g_waiting = DhcpWaiting::OFFER;
    g_got_nak = false;

    uint8_t buf[sizeof(DhcpMessage)];
    uint16_t len = BuildMessage(buf, DHCP_MSG_DISCOVER, nullptr, nullptr);

    static const uint8_t broadcast_ip[4] = {255, 255, 255, 255};
    if (!UDP::Send(broadcast_ip, DHCP_CLIENT_PORT, DHCP_SERVER_PORT, buf, len)) {
        SerialPort::WriteString("DHCP: failed to send DISCOVER.\n");
        return false;
    }
    SerialPort::WriteString("DHCP: sent DISCOVER.\n");

    uint32_t spins = 0;
    while (g_waiting == DhcpWaiting::OFFER) {
        if (++spins > 20000000) {
            SerialPort::WriteString("DHCP: timeout waiting for OFFER.\n");
            return false;
        }
    }

    SerialPort::WriteString("DHCP: received OFFER for ");
    WriteIp(g_offered_ip);
    SerialPort::WriteString("\n");

    g_waiting = DhcpWaiting::ACK;
    len = BuildMessage(buf, DHCP_MSG_REQUEST, g_offered_ip, g_server_id);
    if (!UDP::Send(broadcast_ip, DHCP_CLIENT_PORT, DHCP_SERVER_PORT, buf, len)) {
        SerialPort::WriteString("DHCP: failed to send REQUEST.\n");
        return false;
    }
    SerialPort::WriteString("DHCP: sent REQUEST.\n");

    spins = 0;
    while (g_waiting == DhcpWaiting::ACK) {
        if (++spins > 20000000) {
            SerialPort::WriteString("DHCP: timeout waiting for ACK.\n");
            return false;
        }
    }

    if (g_got_nak) {
        SerialPort::WriteString("DHCP: server sent NAK.\n");
        return false;
    }

    for (int i = 0; i < 4; i++) NET_OUR_IP[i] = g_offered_ip[i];
    // Fallback jesli serwer (nietypowo) nie dolaczyl opcji Router - w prostych sieciach
    // (jak SLIRP) serwer DHCP i brama to zazwyczaj ten sam adres.
    for (int i = 0; i < 4; i++) NET_GATEWAY_IP[i] = g_have_router ? g_router[i] : g_server_id[i];

    SerialPort::WriteString("DHCP: bound, IP=");
    WriteIp(NET_OUR_IP);
    SerialPort::WriteString(" gateway=");
    WriteIp(NET_GATEWAY_IP);
    SerialPort::WriteString("\n");
    return true;
}

void DHCP::HandleFrame(const uint8_t* data, uint16_t len) {
    uint16_t fixed_size = (uint16_t)(sizeof(DhcpMessage) - sizeof(DhcpMessage::options));
    if (len < fixed_size) return;

    const DhcpMessage* msg = (const DhcpMessage*)data;
    if (msg->op != DHCP_OP_BOOTREPLY) return;
    if (Swap32(msg->magic_cookie) != DHCP_MAGIC_COOKIE) return;
    if (Swap32(msg->xid) != g_xid) return; // nie nasza wymiana

    uint16_t options_len = (uint16_t)(len - fixed_size);
    uint8_t opt_len = 0;
    const uint8_t* type_opt = FindOption(msg->options, options_len, OPT_MESSAGE_TYPE, &opt_len);
    if (!type_opt || opt_len < 1) return;
    uint8_t msg_type = type_opt[0];

    if (msg_type == DHCP_MSG_OFFER && g_waiting == DhcpWaiting::OFFER) {
        for (int i = 0; i < 4; i++) g_offered_ip[i] = msg->yiaddr[i];

        const uint8_t* server_id_opt = FindOption(msg->options, options_len, OPT_SERVER_ID, &opt_len);
        if (server_id_opt && opt_len == 4) {
            for (int i = 0; i < 4; i++) g_server_id[i] = server_id_opt[i];
        }
        const uint8_t* router_opt = FindOption(msg->options, options_len, OPT_ROUTER, &opt_len);
        if (router_opt && opt_len >= 4) {
            for (int i = 0; i < 4; i++) g_router[i] = router_opt[i];
            g_have_router = true;
        }
        g_waiting = DhcpWaiting::NONE;
    } else if (msg_type == DHCP_MSG_ACK && g_waiting == DhcpWaiting::ACK) {
        // yiaddr w ACK potwierdza przydzial - bierzemy go na nowo zamiast ufac wylacznie
        // temu co przyszlo w OFFER, tak jak faktycznie mowi RFC 2131.
        for (int i = 0; i < 4; i++) g_offered_ip[i] = msg->yiaddr[i];
        const uint8_t* router_opt = FindOption(msg->options, options_len, OPT_ROUTER, &opt_len);
        if (router_opt && opt_len >= 4) {
            for (int i = 0; i < 4; i++) g_router[i] = router_opt[i];
            g_have_router = true;
        }
        g_waiting = DhcpWaiting::NONE;
    } else if (msg_type == DHCP_MSG_NAK && g_waiting == DhcpWaiting::ACK) {
        g_got_nak = true;
        g_waiting = DhcpWaiting::NONE;
    }
    // Inne typy/nieoczekiwana faza - po cichu ignorowane (np. spozniony powtorzony OFFER
    // podczas czekania na ACK, przez strazniki g_waiting nie nadpisuje juz ustalonego stanu).
}
