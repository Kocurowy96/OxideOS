#include "tcp.h"
#include "ip.h"
#include "config.h"
#include "../cpu/critical.h"
#include "../serial.h"

#define IP_PROTOCOL_TCP 6

#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10

#define TCP_RX_BUFFER_SIZE 4096

struct TcpHeader {
    uint16_t src_port; // kolejnosc sieciowa
    uint16_t dst_port; // kolejnosc sieciowa
    uint32_t seq;       // kolejnosc sieciowa
    uint32_t ack;        // kolejnosc sieciowa
    uint8_t data_offset; // gorne 4 bity = dlugosc naglowka w slowach 32-bit (5 = 20B, bez opcji)
    uint8_t flags;
    uint16_t window;    // kolejnosc sieciowa
    uint16_t checksum;  // kolejnosc sieciowa
    uint16_t urgent_ptr; // kolejnosc sieciowa
} __attribute__((packed));

static uint16_t Swap16(uint16_t v) {
    return (uint16_t)((v >> 8) | (v << 8));
}

static uint32_t Swap32(uint32_t v) {
    return ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) |
           ((v & 0x00FF0000u) >> 8) | ((v & 0xFF000000u) >> 24);
}

// Ten sam podzial AccumulateSum+FinishChecksum co UDP (Faza 5) - suma kontrolna TCP tez
// wymaga pseudo-naglowka IP (RFC 793 sec. 3.1, ten sam uklad co RFC 768 dla UDP) obok
// samego segmentu, a te dwa bufory nie sasiaduja fizycznie w pamieci.
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

static bool IpEquals(const uint8_t a[4], const uint8_t b[4]) {
    for (int i = 0; i < 4; i++) if (a[i] != b[i]) return false;
    return true;
}

// Stan JEDYNEGO aktywnego polaczenia (Faza 6b: swiadomie jedno na raz, brak roli serwera).
// `volatile` na polach dzielonych miedzy HandleFrame (przerwanie RX) a Connect/SendData/
// Receive/Close (kontekst startowy kernela, petle spinowe) - ten sam powod co
// g_critical_depth w critical.h. g_rx_buffer chroniony EnterCritical/ExitCritical zamiast
// samego volatile, bo to kopiowanie wielu bajtow, nie pojedynczy odczyt/zapis.
enum class TcpState { CLOSED, SYN_SENT, ESTABLISHED, PEER_CLOSED, CLOSING };
static volatile TcpState g_state = TcpState::CLOSED;

static uint8_t g_remote_ip[4] = {0, 0, 0, 0};
static uint16_t g_remote_port = 0;
static uint16_t g_local_port = 0;
static uint16_t g_next_local_port = 49152; // pierwszy port efemeryczny wg IANA

static volatile uint32_t g_our_seq = 0;         // nastepny numer sekwencyjny KTORY WYSLEMY
static volatile uint32_t g_their_next_seq = 0;  // nastepny numer sekwencyjny ktorego oczekujemy (wartosc naszego pola ack)
static volatile uint32_t g_highest_acked = 0;   // najwyzszy ack jaki druga strona nam potwierdzila
static volatile uint32_t g_our_fin_seq = 0;     // numer sekwencyjny naszego FIN (do sprawdzenia czy juz potwierdzony)
static volatile bool g_peer_fin_received = false;

static uint8_t g_rx_buffer[TCP_RX_BUFFER_SIZE];
static volatile uint16_t g_rx_len = 0;

static bool SendSegment(uint8_t flags, uint32_t seq, uint32_t ack, const uint8_t* payload, uint16_t payload_len) {
    uint8_t seg[1500];
    uint16_t seg_len = (uint16_t)(sizeof(TcpHeader) + payload_len);
    if (seg_len > sizeof(seg)) return false;

    TcpHeader* hdr = (TcpHeader*)seg;
    hdr->src_port = Swap16(g_local_port);
    hdr->dst_port = Swap16(g_remote_port);
    hdr->seq = Swap32(seq);
    hdr->ack = Swap32(ack);
    hdr->data_offset = (uint8_t)(5 << 4); // 5*4=20B, bez opcji
    hdr->flags = flags;
    hdr->window = Swap16(4096); // stale, male okno - brak prawdziwej kontroli przeplywu (swiadome uproszczenie)
    hdr->checksum = 0;
    hdr->urgent_ptr = 0;
    for (uint16_t i = 0; i < payload_len; i++) seg[sizeof(TcpHeader) + i] = payload[i];

    uint8_t pseudo[12];
    for (int i = 0; i < 4; i++) pseudo[i] = NET_OUR_IP[i];
    for (int i = 0; i < 4; i++) pseudo[4 + i] = g_remote_ip[i];
    pseudo[8] = 0;
    pseudo[9] = IP_PROTOCOL_TCP;
    pseudo[10] = (uint8_t)(seg_len >> 8);
    pseudo[11] = (uint8_t)(seg_len & 0xFF);

    uint32_t sum = 0;
    AccumulateSum(pseudo, sizeof(pseudo), &sum);
    AccumulateSum(seg, seg_len, &sum);
    hdr->checksum = Swap16(FinishChecksum(sum));

    return IP::Send(g_remote_ip, IP_PROTOCOL_TCP, seg, seg_len);
}

bool TCP::Connect(const uint8_t dst_ip[4], uint16_t dst_port) {
    if (g_state != TcpState::CLOSED) {
        SerialPort::WriteString("TCP: polaczenie juz aktywne - Faza 6b obsluguje tylko jedno na raz.\n");
        return false;
    }

    for (int i = 0; i < 4; i++) g_remote_ip[i] = dst_ip[i];
    g_remote_port = dst_port;
    g_local_port = g_next_local_port++;
    // ISN stala, nie losowa - ten sam wybor co xid w DHCP (Faza 6a): klient laczy sie
    // doraznie, nie potrzeba odpornosci na spoofing w prywatnej sieci testowej SLIRP.
    g_our_seq = 0x1000;
    g_their_next_seq = 0;
    g_highest_acked = 0;
    g_peer_fin_received = false;
    g_rx_len = 0;
    g_state = TcpState::SYN_SENT;

    if (!SendSegment(TCP_FLAG_SYN, g_our_seq, 0, nullptr, 0)) {
        g_state = TcpState::CLOSED;
        return false;
    }

    SerialPort::WriteString("TCP: sent SYN to ");
    WriteIp(dst_ip);
    SerialPort::WriteString(":");
    WriteDecimal(dst_port);
    SerialPort::WriteString("\n");

    uint32_t spins = 0;
    while (g_state == TcpState::SYN_SENT) {
        if (++spins > 20000000) {
            SerialPort::WriteString("TCP: timeout waiting for SYN-ACK.\n");
            g_state = TcpState::CLOSED;
            return false;
        }
    }

    SerialPort::WriteString("TCP: connection established.\n");
    return true;
}

bool TCP::SendData(const uint8_t* data, uint16_t len) {
    if (g_state != TcpState::ESTABLISHED) return false;
    if (len == 0) return true;

    uint32_t seq = g_our_seq;
    if (!SendSegment(TCP_FLAG_ACK | TCP_FLAG_PSH, seq, g_their_next_seq, data, len)) return false;

    uint32_t target_ack = seq + len;
    uint32_t spins = 0;
    while (g_highest_acked < target_ack) {
        if (++spins > 20000000) {
            SerialPort::WriteString("TCP: timeout waiting for data ack.\n");
            return false;
        }
    }
    g_our_seq = target_ack;
    return true;
}

uint16_t TCP::Receive(uint8_t* buf, uint16_t max_len) {
    uint32_t spins = 0;
    while (g_rx_len == 0 && !g_peer_fin_received) {
        if (g_state == TcpState::CLOSED) return 0;
        if (++spins > 20000000) {
            SerialPort::WriteString("TCP: receive timeout.\n");
            return 0;
        }
    }

    if (g_rx_len == 0) return 0; // bufor pusty i druga strona juz zamknela - EOF

    EnterCritical();
    uint16_t copy_len = g_rx_len < max_len ? g_rx_len : max_len;
    for (uint16_t i = 0; i < copy_len; i++) buf[i] = g_rx_buffer[i];
    uint16_t remaining = (uint16_t)(g_rx_len - copy_len);
    for (uint16_t i = 0; i < remaining; i++) g_rx_buffer[i] = g_rx_buffer[copy_len + i];
    g_rx_len = remaining;
    ExitCritical();

    return copy_len;
}

void TCP::Close() {
    if (g_state != TcpState::ESTABLISHED && g_state != TcpState::PEER_CLOSED) return;

    g_our_fin_seq = g_our_seq;
    SendSegment(TCP_FLAG_FIN | TCP_FLAG_ACK, g_our_seq, g_their_next_seq, nullptr, 0);
    g_our_seq = g_our_seq + 1; // FIN zajmuje jeden numer sekwencyjny
    g_state = TcpState::CLOSING;

    uint32_t spins = 0;
    while (g_highest_acked <= g_our_fin_seq) {
        if (++spins > 20000000) {
            SerialPort::WriteString("TCP: timeout waiting for FIN ack.\n");
            break;
        }
    }

    // Krotkie dodatkowe czekanie na FIN drugiej strony jesli jeszcze nie przyszedl (np.
    // zamykamy pierwsi) - ale nie wisimy w nieskonczonosc, jesli druga strona juz zamknela
    // ciszej niz oczekiwano albo nie odpowie wcale.
    spins = 0;
    while (!g_peer_fin_received) {
        if (++spins > 5000000) break;
    }

    g_state = TcpState::CLOSED;
    SerialPort::WriteString("TCP: connection closed.\n");
}

void TCP::HandleFrame(const uint8_t src_ip[4], const uint8_t* data, uint16_t len) {
    if (len < sizeof(TcpHeader)) return;
    if (g_state == TcpState::CLOSED) return;

    const TcpHeader* hdr = (const TcpHeader*)data;

    // Tylko ruch nalezacy do naszego jedynego aktywnego polaczenia - wszystko inne odsiewamy.
    if (!IpEquals(src_ip, g_remote_ip)) return;
    if (Swap16(hdr->src_port) != g_remote_port) return;
    if (Swap16(hdr->dst_port) != g_local_port) return;

    uint8_t header_len = (uint8_t)((hdr->data_offset >> 4) * 4);
    if (header_len < sizeof(TcpHeader) || header_len > len) return;
    const uint8_t* payload = data + header_len;
    uint16_t payload_len = (uint16_t)(len - header_len);

    uint32_t seq = Swap32(hdr->seq);
    uint32_t ack = Swap32(hdr->ack);
    uint8_t flags = hdr->flags;

    if (flags & TCP_FLAG_RST) {
        SerialPort::WriteString("TCP: received RST, closing.\n");
        g_state = TcpState::CLOSED;
        return;
    }

    if (g_state == TcpState::SYN_SENT) {
        if ((flags & TCP_FLAG_SYN) && (flags & TCP_FLAG_ACK) && ack == g_our_seq + 1) {
            g_their_next_seq = seq + 1; // SYN drugiej strony zajmuje jeden numer sekwencyjny
            g_our_seq = g_our_seq + 1;
            SendSegment(TCP_FLAG_ACK, g_our_seq, g_their_next_seq, nullptr, 0);
            g_state = TcpState::ESTABLISHED;
        }
        return;
    }

    if (g_state == TcpState::ESTABLISHED || g_state == TcpState::CLOSING || g_state == TcpState::PEER_CLOSED) {
        if ((flags & TCP_FLAG_ACK) && ack > g_highest_acked) {
            g_highest_acked = ack;
        }

        if (payload_len > 0 && seq == g_their_next_seq) {
            EnterCritical();
            uint16_t copy_len = payload_len;
            if ((uint32_t)g_rx_len + copy_len > TCP_RX_BUFFER_SIZE) {
                // Bufor pelny - obcinamy nadmiar (brak prawdziwej kontroli przeplywu,
                // swiadome uproszczenie zakresu Fazy 6b, patrz tcp.h).
                copy_len = (uint16_t)(TCP_RX_BUFFER_SIZE - g_rx_len);
            }
            for (uint16_t i = 0; i < copy_len; i++) g_rx_buffer[g_rx_len + i] = payload[i];
            g_rx_len = (uint16_t)(g_rx_len + copy_len);
            ExitCritical();
            g_their_next_seq = g_their_next_seq + payload_len;
        }

        if (flags & TCP_FLAG_FIN) {
            g_their_next_seq = g_their_next_seq + 1; // FIN tez zajmuje jeden numer sekwencyjny
            g_peer_fin_received = true;
            if (g_state == TcpState::ESTABLISHED) g_state = TcpState::PEER_CLOSED;
        }

        // ACK kazdego segmentu z danymi i/lub FIN (kumulacyjny) - czysty ACK bez danych/FIN
        // nie wymaga odpowiedzi.
        if (payload_len > 0 || (flags & TCP_FLAG_FIN)) {
            SendSegment(TCP_FLAG_ACK, g_our_seq, g_their_next_seq, nullptr, 0);
        }
    }
}
