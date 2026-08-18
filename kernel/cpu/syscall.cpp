#include "syscall.h"
#include "../serial.h"
#include "../proc/sched.h"
#include "../gui/window.h"
#include "../gui/compositor.h"
#include "../mem/pmm.h"
#include "../mem/vmm.h"
#include "../limine.h"
#include "../fs/vfs.h"

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
    } else if (syscall_num == 3) { // sys_write_file
        const char* path = (const char*)regs->rdi;
        const uint8_t* buf = (const uint8_t*)regs->rsi;
        uint32_t size = (uint32_t)regs->rdx;
        regs->rax = VFS::WriteFile(path, buf, size) ? 1 : 0;
    } else if (syscall_num == 50) { // sys_create_window
        // rdi: title (const char*), rsi: width, rdx: height, r10: x, r8: y, r9: CreateWindowResult* pointer
        const char* title = (const char*)regs->rdi;
        int w = regs->rsi;
        int h = regs->rdx;
        int x = regs->r10;
        int y = regs->r8;
        uint64_t res_ptr = regs->r9;
        
        static Window sys_windows[32] = {
            Window(0,0,0,0,""), Window(0,0,0,0,""), Window(0,0,0,0,""), Window(0,0,0,0,""),
            Window(0,0,0,0,""), Window(0,0,0,0,""), Window(0,0,0,0,""), Window(0,0,0,0,""),
            Window(0,0,0,0,""), Window(0,0,0,0,""), Window(0,0,0,0,""), Window(0,0,0,0,""),
            Window(0,0,0,0,""), Window(0,0,0,0,""), Window(0,0,0,0,""), Window(0,0,0,0,""),
            Window(0,0,0,0,""), Window(0,0,0,0,""), Window(0,0,0,0,""), Window(0,0,0,0,""),
            Window(0,0,0,0,""), Window(0,0,0,0,""), Window(0,0,0,0,""), Window(0,0,0,0,""),
            Window(0,0,0,0,""), Window(0,0,0,0,""), Window(0,0,0,0,""), Window(0,0,0,0,""),
            Window(0,0,0,0,""), Window(0,0,0,0,""), Window(0,0,0,0,""), Window(0,0,0,0,"")
        };
        static int sys_windows_count = 0;
        
        if (sys_windows_count >= 32) {
            regs->rax = 0; // Error
            return;
        }
        
        Window* win = &sys_windows[sys_windows_count++];
        *win = Window(x, y, w, h, title);
        
        // Allocate physical memory for the framebuffer
        size_t size = w * h * 4;
        size_t pages = (size + 4095) / 4096;
        void* phys = PMM::AllocatePages(pages);
        
        extern volatile struct limine_hhdm_request hhdm_request;
        win->fb_buffer = (uint32_t*)((uint64_t)phys + hhdm_request.response->offset);
        
        // Map into Userspace
        uint64_t vaddr = 0x800000000000 + win->id * 0x1000000;
        for (size_t i = 0; i < pages; i++) {
            VMM::MapPage((uint64_t)phys + i * 4096, vaddr + i * 4096, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
        }
        
        Compositor::AddWindow(win);
        
        // Return results
        if (res_ptr) {
            int* p_id = (int*)res_ptr;
            uint64_t* p_fb = (uint64_t*)(res_ptr + 8);
            *p_id = win->id;
            *p_fb = vaddr;
        }
        regs->rax = win->id;
    } else if (syscall_num == 51) { // sys_update_window
        // Window is updated in memory, Compositor picks it up automatically on Render()
        // Here we could just force a redraw or do nothing if Render loops continuously.
        regs->rax = 0;
    } else if (syscall_num == 52) { // sys_get_event
        // rdi: window_id, rsi: Event* (w userspace)
        int win_id = regs->rdi;
        uint64_t ev_ptr = regs->rsi;
        Window* win = Compositor::GetWindowById(win_id);
        if (win && ev_ptr) {
            Window::Event ev;
            if (win->PopEvent(&ev)) {
                Window::Event* u_ev = (Window::Event*)ev_ptr;
                *u_ev = ev;
                regs->rax = 1;
            } else {
                regs->rax = 0;
            }
        } else {
            regs->rax = 0;
        }
    } else {
        SerialPort::WriteString("Syscall: Unknown syscall number!\n");
    }
}
