#include "isr.h"
#include "syscall.h"
#include "../serial.h"
#include "../gui/osod.h"
#include "../drivers/pic.h"
#include "../drivers/pit.h"
#include "../drivers/ps2_kbd.h"
#include "../drivers/ps2_mouse.h"
#include "../proc/sched.h"
#include "critical.h"

extern "C" Registers* isr_handler(Registers* regs) {
    if (regs->int_no < 32) {
        SerialPort::WriteString("OxideOS: Kernel Panic! (OSOD)\n");
        SerialPort::WriteString("Exception Number: ");
        char buf[16];
        auto itoa = [](uint64_t v, char* b, int base) {
            char* p = b;
            if (v == 0) { *p++ = '0'; *p = 0; return b; }
            while(v) { int rem = v % base; *p++ = (rem < 10) ? rem + '0' : rem - 10 + 'a'; v /= base; }
            *p = 0;
            char* p1 = b; char* p2 = p - 1;
            while(p1 < p2) { char tmp = *p1; *p1 = *p2; *p2 = tmp; p1++; p2--; }
            return b;
        };
        SerialPort::WriteString(itoa(regs->int_no, buf, 10));
        SerialPort::WriteString("\nRIP: 0x");
        SerialPort::WriteString(itoa(regs->rip, buf, 16));
        SerialPort::WriteString("\nError Code: 0x");
        SerialPort::WriteString(itoa(regs->err_code, buf, 16));
        SerialPort::WriteString("\nHalting.\n");
        
        OSOD::Draw(regs);
        asm volatile("cli; hlt");
    }
    
    uint64_t int_no = regs->int_no;
    
    if (int_no == 0x80) {
        // Interrupt gates juz wchodza tu z IF=0, ale sterowniki (ATA itd.) wewnatrz
        // syscalli robia wlasne EnterCritical/ExitCritical, ktore przy zejsciu do 0
        // wlaczaja przerwania *w trakcie* obslugi syscalla. Jesli w tym oknie trafi
        // tick timera, Scheduler::Schedule() zapisze jako "punkt wznowienia" taska
        // stan w SRODKU obslugi syscalla na wspoldzielonym stosie jadra (brak TSS
        // per-task) zamiast normalnego stanu Ring3 - i to jest to, co psulo okna przy
        // 2 rownoczesnie dzialajacych aplikacjach. Owijamy caly syscall, zeby zaden
        // "wewnetrzny" sti nie mogl otworzyc tego okna.
        EnterCritical();
        Syscall::Handler(regs);
        ExitCritical();
    }
    
    if (int_no >= 32 && int_no <= 47) {
        if (int_no == 32) {
            PIT::Tick();
            regs = Scheduler::Schedule(regs);
        } else if (int_no == 33) {
            Keyboard::HandleInterrupt();
        } else if (int_no == 44) {
            Mouse::HandleInterrupt();
        }
        PIC::SendEOI(int_no - 32);
    }
    
    return regs;
}
