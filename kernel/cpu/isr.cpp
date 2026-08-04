#include "isr.h"
#include "../serial.h"
#include "../gui/osod.h"
#include "../drivers/pic.h"
#include "../drivers/pit.h"
#include "../proc/sched.h"

extern "C" Registers* isr_handler(Registers* regs) {
    if (regs->int_no < 32) {
        SerialPort::WriteString("OxideOS: Kernel Panic! (OSOD)\n");
        SerialPort::WriteString("Exception Number: ");
        char buf[16] = {0};
        int val = regs->int_no;
        int i = 0;
        if (val == 0) buf[i++] = '0';
        while (val > 0) { buf[i++] = '0' + (val % 10); val /= 10; }
        for (int j = i - 1; j >= 0; j--) SerialPort::WriteChar(buf[j]);
        SerialPort::WriteString("\nHalting.\n");
        
        OSOD::Draw(regs);
        asm volatile("cli; hlt");
    }
    
    uint64_t int_no = regs->int_no;
    
    if (int_no >= 32 && int_no <= 47) {
        if (int_no == 32) {
            PIT::Tick();
            regs = Scheduler::Schedule(regs);
        }
        PIC::SendEOI(int_no - 32);
    }
    
    return regs;
}
