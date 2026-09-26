#include "rtl8139.h"
#include "pci.h"
#include "../serial.h"
#include "../cpu/io.h"

// Rejestry RTL8139 uzywane w tej fazie (patrz OSDev Wiki "RTL8139 Programming"):
#define RTL_REG_MAC0     0x00 // IDR0-IDR5, 6 bajtow adresu MAC
#define RTL_REG_CONFIG1  0x52
#define RTL_REG_CR       0x37 // Command Register - bit4 = RST (soft reset)
#define RTL_CR_RST       0x10

static uint16_t io_base = 0;
static uint8_t mac_addr[6] = {0};
static bool present = false;

static void WriteHexByte(uint8_t val) {
    static const char hex[] = "0123456789ABCDEF";
    SerialPort::WriteChar(hex[(val >> 4) & 0xF]);
    SerialPort::WriteChar(hex[val & 0xF]);
}

bool RTL8139::IsPresent() {
    return present;
}

void RTL8139::GetMacAddress(uint8_t out_mac[6]) {
    for (int i = 0; i < 6; i++) out_mac[i] = mac_addr[i];
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

    present = true;
}
