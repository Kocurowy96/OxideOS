#include "isr.h"
#include "../serial.h"

extern "C" void isr_handler(Registers* regs) {
    if (regs->int_no < 32) {
        SerialPort::WriteString("OxideOS: Kernel Panic! (OSOD)\n");
        SerialPort::WriteString("Exception Number: ");
        // simple int to string for debug
        char buf[16] = {0};
        int val = regs->int_no;
        int i = 0;
        if (val == 0) buf[i++] = '0';
        while (val > 0) { buf[i++] = '0' + (val % 10); val /= 10; }
        for (int j = i - 1; j >= 0; j--) SerialPort::WriteChar(buf[j]);
        SerialPort::WriteString("\nHalting.\n");
        asm volatile("cli; hlt");
    }
}
