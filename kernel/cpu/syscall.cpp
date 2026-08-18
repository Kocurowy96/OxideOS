#include "syscall.h"
#include "../serial.h"
#include "../proc/sched.h"

void Syscall::Handler(Registers* regs) {
    uint64_t syscall_num = regs->rax;
    
    if (syscall_num == 1) { // sys_print
        char* str = (char*)regs->rdi;
        SerialPort::WriteString(str);
    } else if (syscall_num == 2) { // sys_exit
        SerialPort::WriteString("Syscall: sys_exit called. Killing task...\n");
        Scheduler::KillCurrentTask();
        // Since we killed the task, we need to schedule immediately.
        // But the interrupt handler returns `regs`.
        // We can just loop until the timer interrupt preempts us.
        asm volatile("sti");
        while(1) {
            asm volatile("hlt");
        }
    } else {
        SerialPort::WriteString("Syscall: Unknown syscall number!\n");
    }
}
