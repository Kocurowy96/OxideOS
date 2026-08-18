#include "rtc.h"
#include "../serial.h"

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) : "memory");
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

uint8_t RTC::ReadRegister(uint8_t reg) {
    outb(0x70, (1 << 7) | reg); // NMI disable bit set
    return inb(0x71);
}

static void print_hex(uint8_t val) {
    const char* hex = "0123456789ABCDEF";
    SerialPort::WriteChar(hex[val >> 4]);
    SerialPort::WriteChar(hex[val & 0x0F]);
}

void RTC::PrintTime() {
    uint8_t second = ReadRegister(0x00);
    uint8_t minute = ReadRegister(0x02);
    uint8_t hour = ReadRegister(0x04);
    uint8_t day = ReadRegister(0x07);
    uint8_t month = ReadRegister(0x08);
    uint8_t year = ReadRegister(0x09);

    SerialPort::WriteString("Current RTC Time (BCD): 20");
    print_hex(year); SerialPort::WriteChar('-');
    print_hex(month); SerialPort::WriteChar('-');
    print_hex(day); SerialPort::WriteChar(' ');
    print_hex(hour); SerialPort::WriteChar(':');
    print_hex(minute); SerialPort::WriteChar(':');
    print_hex(second); SerialPort::WriteString("\n");
}

uint8_t RTC::GetHour() {
    return ReadRegister(0x04);
}

uint8_t RTC::GetMinute() {
    return ReadRegister(0x02);
}

uint8_t RTC::GetDay() {
    return ReadRegister(0x07);
}

uint8_t RTC::GetMonth() {
    return ReadRegister(0x08);
}

uint8_t RTC::GetYear() {
    return ReadRegister(0x09);
}
