#include "rtl8139.h"
#include "pci.h"
#include "pic.h"
#include "../serial.h"
#include "../cpu/io.h"
#include "../mem/pmm.h"
#include "../limine.h"

extern volatile struct limine_hhdm_request hhdm_request;

// Rejestry RTL8139 uzywane w tej fazie (patrz OSDev Wiki "RTL8139 Programming"):
#define RTL_REG_MAC0     0x00 // IDR0-IDR5, 6 bajtow adresu MAC
#define RTL_REG_RBSTART  0x30 // Fizyczny adres bufora RX (4 bajty)
#define RTL_REG_CONFIG1  0x52
#define RTL_REG_CR       0x37 // Command Register - bit4=RST, bit3=RE, bit2=TE
#define RTL_REG_IMR      0x3C // Interrupt Mask Register (2 bajty)
#define RTL_REG_ISR      0x3E // Interrupt Status Register (2 bajty) - "write 1 to clear"
#define RTL_REG_RCR      0x44 // Receive Config Register (4 bajty)

#define RTL_CR_RST       0x10
#define RTL_CR_RE        0x08
#define RTL_CR_TE        0x04

#define RTL_ISR_ROK      0x01 // Receive OK
#define RTL_ISR_TOK      0x04 // Transmit OK

// Bufor RX: klasyczny rozmiar z tutoriali OSDev - 8192B logiczny pierscien + 16B naglowek
// + 1500B zapasu (RTL8139 moze dopisac troche za koniec "logicznego" bufora zanim
// zawinie z powrotem, nawet z ustawionym WRAP). Zaokraglone w gore do pelnych stron.
#define RX_BUFFER_LOGICAL_SIZE (8192 + 16 + 1500)
#define RX_BUFFER_PAGES 3 // 3*4096 = 12288 > 9708

static uint16_t io_base = 0;
static uint8_t mac_addr[6] = {0};
static bool present = false;
static uint8_t irq_line = 0;
static void* rx_buffer_phys = nullptr;

static void WriteHexByte(uint8_t val) {
    static const char hex[] = "0123456789ABCDEF";
    SerialPort::WriteChar(hex[(val >> 4) & 0xF]);
    SerialPort::WriteChar(hex[val & 0xF]);
}

static void WriteDecimal(uint32_t val) {
    char buf[12];
    int i = 0;
    if (val == 0) buf[i++] = '0';
    while (val > 0) { buf[i++] = '0' + (val % 10); val /= 10; }
    while (i > 0) SerialPort::WriteChar(buf[--i]);
}

bool RTL8139::IsPresent() {
    return present;
}

void RTL8139::GetMacAddress(uint8_t out_mac[6]) {
    for (int i = 0; i < 6; i++) out_mac[i] = mac_addr[i];
}

uint8_t RTL8139::GetIrqLine() {
    return irq_line;
}

void RTL8139::HandleInterrupt() {
    uint16_t status = inw(io_base + RTL_REG_ISR);
    // "Write 1 to clear" - odpisujemy dokladnie te bity ktore widzielismy ustawione.
    outw(io_base + RTL_REG_ISR, status);

    if (status & RTL_ISR_ROK) {
        SerialPort::WriteString("RTL8139: RX interrupt (not parsed yet - Faza 2).\n");
    }
    if (status & RTL_ISR_TOK) {
        SerialPort::WriteString("RTL8139: TX interrupt.\n");
    }
}

void RTL8139::Init() {
    uint8_t bus, slot, func;
    if (!PCI::FindDevice(0x10EC, 0x8139, &bus, &slot, &func)) {
        SerialPort::WriteString("RTL8139: Not found.\n");
        return;
    }

    SerialPort::WriteString("RTL8139: Found network card.\n");

    // Bus Mastering (bit2) + I/O Space (bit0) w rejestrze Command (offset 0x04)
    uint16_t cmd = PCI::ConfigRead16(bus, slot, func, 0x04);
    cmd |= 0x05;
    PCI::ConfigWrite16(bus, slot, func, 0x04, cmd);

    // BAR0 - port I/O (bit0 seta ustawiony = mapowanie I/O, dolne 2 bity to nie adres)
    io_base = (uint16_t)(PCI::ConfigRead32(bus, slot, func, 0x10) & 0xFFFC);

    // Linia IRQ przydzielona przez BIOS/QEMU (offset 0x3C w przestrzeni konfiguracyjnej PCI,
    // nie mylic z rejestrem IMR karty pod tym samym numerem 0x3C w przestrzeni I/O) - nie
    // znamy jej z gory jak przy klawiaturze(1)/myszy(12), trzeba odczytac w runtime.
    irq_line = PCI::ConfigRead8(bus, slot, func, 0x3C);

    // Power on (budzi karte z ewentualnego stanu oszczedzania energii)
    outb(io_base + RTL_REG_CONFIG1, 0x00);

    // Soft reset - ustaw RST, czekaj az karta sama go wyzeruje (reset zakonczony)
    outb(io_base + RTL_REG_CR, RTL_CR_RST);
    uint32_t spins = 0;
    while (inb(io_base + RTL_REG_CR) & RTL_CR_RST) {
        if (++spins > 1000000) {
            SerialPort::WriteString("RTL8139: Reset timeout.\n");
            return;
        }
    }

    for (int i = 0; i < 6; i++) mac_addr[i] = inb(io_base + RTL_REG_MAC0 + i);

    SerialPort::WriteString("RTL8139: MAC address: ");
    for (int i = 0; i < 6; i++) {
        WriteHexByte(mac_addr[i]);
        if (i < 5) SerialPort::WriteChar(':');
    }
    SerialPort::WriteString("\n");

    // Faza 1b: bufor RX + wlaczenie RX/TX + IRQ.
    rx_buffer_phys = PMM::AllocatePages(RX_BUFFER_PAGES);
    if (!rx_buffer_phys) {
        SerialPort::WriteString("RTL8139: Failed to allocate RX buffer.\n");
        return;
    }
    // RBSTART chce fizycznego adresu (karta robi DMA bezposrednio do RAM, nie zna
    // wirtualnych adresow jadra) - w odroznieniu od hhdm-owego wskaznika ktorego
    // uzylibysmy do samego CZYTANIA zawartosci bufora z poziomu CPU (Faza 2).
    outl(io_base + RTL_REG_RBSTART, (uint32_t)(uint64_t)rx_buffer_phys);

    // RCR: AAP+APM+AM+AB (akceptuj wszystko - wszystkie pakiety/dopasowanie fizyczne/
    // multicast/broadcast) + WRAP (bit7) - upraszcza Faze 2, karta sama pilnuje zeby
    // pojedynczy pakiet nie byl rozdzielony przez zawiniecie pierscienia.
    outl(io_base + RTL_REG_RCR, 0x0F | (1 << 7));

    // Wlacz odbiornik i nadajnik.
    outb(io_base + RTL_REG_CR, RTL_CR_RE | RTL_CR_TE);

    // IMR: przerwania Receive OK + Transmit OK.
    outw(io_base + RTL_REG_IMR, RTL_ISR_ROK | RTL_ISR_TOK);

    PIC::ClearMask(irq_line);

    SerialPort::WriteString("RTL8139: RX/TX enabled, IRQ line ");
    WriteDecimal(irq_line);
    SerialPort::WriteString(".\n");

    present = true;
}
