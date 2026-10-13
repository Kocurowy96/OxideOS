#include "isr.h"
#include "syscall.h"
#include "../serial.h"
#include "../gui/osod.h"
#include "../drivers/pic.h"
#include "../drivers/pit.h"
#include "../drivers/ps2_kbd.h"
#include "../drivers/ps2_mouse.h"
#include "../drivers/rtl8139.h"
#include "../proc/sched.h"
#include "critical.h"
#include "gdt.h"

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
        // RSP/CS w momencie faultu - przy page faulcie w kodzie liczacym offset wzgledem
        // wlasnego stosu (np. "mov 0x4(%rsp),%eax") sam CR2 nie mowi czy winny jest
        // zepsuty wskaznik/offset czy RSP (patrz notatka w TASKS.md) - RSP rozstrzyga.
        SerialPort::WriteString("\nRSP: 0x");
        SerialPort::WriteString(itoa(regs->rsp, buf, 16));
        SerialPort::WriteString("\nCS: 0x");
        SerialPort::WriteString(itoa(regs->cs, buf, 16));
        if (regs->int_no == 14) {
            uint64_t cr2;
            asm volatile("mov %%cr2, %0" : "=r"(cr2));
            SerialPort::WriteString("\nCR2 (faulting address): 0x");
            SerialPort::WriteString(itoa(cr2, buf, 16));

            // CR2 wzgledem RSP faultujacego kontekstu, ze znakiem - odrozni "fault na
            // offsecie wzgledem wlasnego stosu" (np. "mov 0x4(%rsp),%eax"/"ret") od
            // czegos niezwiazanego z samym stosem (patrz notatka w TASKS.md, 2026-10-12).
            int64_t cr2_rsp_delta = (int64_t)cr2 - (int64_t)regs->rsp;
            SerialPort::WriteString("\nCR2 - RSP: ");
            if (cr2_rsp_delta < 0) {
                SerialPort::WriteString("-0x");
                SerialPort::WriteString(itoa((uint64_t)(-cr2_rsp_delta), buf, 16));
            } else {
                SerialPort::WriteString("0x");
                SerialPort::WriteString(itoa((uint64_t)cr2_rsp_delta, buf, 16));
            }

            // Stos jadra uzywany PRZY TYM FAULCIE: "regs" to dokladnie RSP w momencie
            // wejscia do isr_handler (isr_common_stub robi "movq %rsp, %rdi" tuz przed
            // "call isr_handler" - to adres o 8 bajtow nad biezacym %rsp w tej funkcji,
            // czyli praktycznie szczyt uzywanej czesci stosu jadra przy tym zdarzeniu).
            // kernel_stack_top(current) to oczekiwana gorna granica wedlug schedulera dla
            // biezacego taska, TSS.rsp0(actual) to co faktycznie jest zaladowane w
            // sprzecie w tej chwili - rozjazd miedzy nimi oznacza zepsuta synchronizacje
            // TSS.rsp0 (nie sam przelacznik taskow), a "regs" blisko/pod
            // kernel_stack_top-32768 oznacza przepelnienie stosu jadra (patrz TASKS.md,
            // "Konkretny nastepny krok" z sesji 2026-10-12).
            uint64_t expected_top = Scheduler::GetCurrentKernelStackTop();
            uint64_t actual_rsp0 = GDT::GetTSSStack();
            SerialPort::WriteString("\nKernel RSP (isr_handler entry): 0x");
            SerialPort::WriteString(itoa((uint64_t)regs, buf, 16));
            SerialPort::WriteString("\nKernel stack top (expected, current task): 0x");
            SerialPort::WriteString(itoa(expected_top, buf, 16));
            SerialPort::WriteString("\nKernel stack bottom (expected, 32KiB): 0x");
            SerialPort::WriteString(itoa(expected_top - 32768, buf, 16));
            SerialPort::WriteString("\nTSS.rsp0 (actual, hardware): 0x");
            SerialPort::WriteString(itoa(actual_rsp0, buf, 16));
        }
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
        } else if (RTL8139::IsPresent() && int_no == 32 + RTL8139::GetIrqLine()) {
            // Linia IRQ karty sieciowej jest przydzielana w runtime przez PCI (nie znana
            // z gory jak przy klawiaturze(1)/myszy(12)), stad porownanie zamiast stalej.
            RTL8139::HandleInterrupt();
        }
        PIC::SendEOI(int_no - 32);
    }
    
    return regs;
}
