#include "syscall.h"
#include "../serial.h"
#include "../proc/sched.h"
#include "../gui/window.h"
#include "../gui/compositor.h"
#include "../mem/pmm.h"
#include "../mem/vmm.h"
#include "../limine.h"
#include "../fs/vfs.h"
#include "../drivers/rtc.h"
#include "../gui/bmp.h"
#include "../gui/fb.h"
#include "../drivers/ac97.h"
#include "critical.h"

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

// Oznacza okna do usunięcia - Compositor sprząta je na początku następnej klatki
static void MarkTaskWindowsForRemoval(uint64_t task_id) {
    for (int i = 0; i < 32; i++) {
        if (sys_windows[i].active && sys_windows[i].owner_task_id == task_id) {
            sys_windows[i].pending_remove = true;
        }
    }
}

// Wywoływane przez Compositor po bezpiecznym zakończeniu klatki
void Syscall::FreeWindowMemory(Window* win) {
    if (!win || !win->phys_fb_buffer) return;
    int w = win->width;
    int h = win->height;
    size_t size = (size_t)w * h * 4;
    size_t pages = (size + 4095) / 4096;
    PMM::FreePages(win->phys_fb_buffer, pages);
    win->phys_fb_buffer = nullptr;
    win->fb_buffer = nullptr;
    win->active = false;
    win->pending_remove = false;
}

void Syscall::Handler(Registers* regs) {
    uint64_t syscall_num = regs->rax;
    
    if (syscall_num == 1) { // sys_print
        char* str = (char*)regs->rdi;
        SerialPort::WriteString(str);
    } else if (syscall_num == 2) { // sys_exit
        SerialPort::WriteString("Syscall: sys_exit called. Killing task...\n");
        // Oznacz okna jako do usunięcia - Compositor zrobi to bezpiecznie między klatkami
        MarkTaskWindowsForRemoval(Scheduler::GetCurrentTaskId());
        Scheduler::KillCurrentTask();
        // sys_exit nigdy nie wraca do isr_handler (wiec jego ExitCritical() po
        // Syscall::Handler() by sie nie wykonalo) - musimy sami zbalansowac
        // EnterCritical() z wejscia do syscalla, inaczej licznik z critical.h
        // zostaje trwale przesuniety i przerwania koncza sie zablokowane na dobre.
        ExitCritical();
        while(1) {
            asm volatile("hlt");
        }
    } else if (syscall_num == 3) { // sys_write_file
        const char* path = (const char*)regs->rdi;
        const uint8_t* buf = (const uint8_t*)regs->rsi;
        uint32_t size = (uint32_t)regs->rdx;
        regs->rax = VFS::WriteFile(path, buf, size) ? 1 : 0;
    } else if (syscall_num == 4) { // sys_get_time
        struct DateTime {
            uint8_t year, month, day, hour, minute, second;
        };
        DateTime* dt = (DateTime*)regs->rdi;
        if (dt) {
            auto BcdToBin = [](uint8_t bcd) { return ((bcd >> 4) * 10) + (bcd & 0x0F); };
            dt->year = BcdToBin(RTC::GetYear());
            dt->month = BcdToBin(RTC::GetMonth());
            dt->day = BcdToBin(RTC::GetDay());
            dt->hour = BcdToBin(RTC::GetHour());
            dt->minute = BcdToBin(RTC::GetMinute());
            dt->second = 0;
            regs->rax = 1;
        } else {
            regs->rax = 0;
        }
    } else if (syscall_num == 6) { // sys_get_mem_info
        uint64_t* total = (uint64_t*)regs->rdi;
        uint64_t* free = (uint64_t*)regs->rsi;
        if (total) *total = PMM::GetTotalMemory();
        if (free) *free = PMM::GetFreeMemory();
        regs->rax = 1;
    } else if (syscall_num == 7) { // sys_get_display_info
        uint32_t* width = (uint32_t*)regs->rdi;
        uint32_t* height = (uint32_t*)regs->rsi;
        uint32_t* bpp = (uint32_t*)regs->rdx;
        if (width) *width = Framebuffer::GetWidth();
        if (height) *height = Framebuffer::GetHeight();
        if (bpp) *bpp = Framebuffer::GetBpp();
        regs->rax = 1;
    } else if (syscall_num == 8) { // sys_play_wav
        const char* path = (const char*)regs->rdi;
        uint8_t* wav_buffer = nullptr;
        uint32_t wav_size = 0;
        if (VFS::ReadFile(path, &wav_buffer, &wav_size)) {
            // Bufor zostaje zywy (nie zwalniamy) - AC97 odtwarza go asynchronicznie przez DMA,
            // dokladnie tak samo jak dzwiek startowy w DesktopTask.
            AC97::PlayWAV(wav_buffer);
            regs->rax = 1;
        } else {
            regs->rax = 0;
        }
    } else if (syscall_num == 9) { // sys_set_volume (0-100)
        uint32_t percent = (uint32_t)regs->rdi;
        if (percent > 100) percent = 100;
        uint8_t atten = (uint8_t)(63 - (percent * 63 / 100)); // 0 = najglosniej, 63 = najciszej
        uint16_t reg_val = ((uint16_t)atten << 8) | atten;
        AC97::WriteCodec(0x18, reg_val); // PCM Out Volume
        regs->rax = 1;
    } else if (syscall_num == 10) { // sys_get_volume
        uint32_t* out_percent = (uint32_t*)regs->rdi;
        uint16_t reg_val = AC97::ReadCodec(0x18);
        uint8_t atten = reg_val & 0x3F;
        if (out_percent) *out_percent = 100 - (atten * 100 / 63);
        regs->rax = 1;
    } else if (syscall_num == 50) { // sys_create_window
        // rdi: title (const char*), rsi: width, rdx: height, r10: x, r8: y, r9: CreateWindowResult* pointer
        const char* title = (const char*)regs->rdi;
        int w = regs->rsi;
        int h = regs->rdx;
        int x = regs->r10;
        int y = regs->r8;
        uint64_t res_ptr = regs->r9;

        // Skan+rezerwacja slotu i cala reszta tworzenia okna musi byc atomowa -
        // inaczej dwa taski moga trafic w ten sam wolny slot (patrz critical.h)
        EnterCritical();

        int slot = -1;
        for (int i = 0; i < 32; i++) {
            if (!sys_windows[i].active) {
                slot = i;
                break;
            }
        }

        if (slot == -1) {
            regs->rax = 0; // Error
            ExitCritical();
            return;
        }

        Window* win = &sys_windows[slot];
        int titlebar_h = 20;
        *win = Window(x, y, w + 4, h + titlebar_h + 2, title);
        
        static int next_id = 100;
        win->id = next_id++;
        win->active = true;
        win->owner_task_id = Scheduler::GetCurrentTaskId();
        
        // Allocate physical memory for the framebuffer
        size_t size = (w + 4) * (h + titlebar_h + 2) * 4;
        size_t pages = (size + 4095) / 4096;
        void* phys = PMM::AllocatePages(pages);
        win->phys_fb_buffer = phys;
        
        extern volatile struct limine_hhdm_request hhdm_request;
        win->fb_buffer = (uint32_t*)((uint64_t)phys + hhdm_request.response->offset);
        
        // Map into Userspace
        uint64_t vaddr = 0x10000000 + win->id * 0x1000000;
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
        ExitCritical();
    } else if (syscall_num == 51) { // sys_update_window
        // Window is updated in memory, Compositor picks it up automatically on Render()
        // Here we could just force a redraw or do nothing if Render loops continuously.
        regs->rax = 0;
    } else if (syscall_num == 53) { // sys_reload_wallpaper
        Compositor::InitWallpaper();
        regs->rax = 1;
    } else if (syscall_num == 54) { // sys_destroy_window
        int win_id = regs->rdi;
        EnterCritical();
        for (int i = 0; i < 32; i++) {
            if (sys_windows[i].id == win_id && sys_windows[i].active) {
                Compositor::RemoveWindow(&sys_windows[i]);
                sys_windows[i].active = false;
                regs->rax = 1;
                ExitCritical();
                return;
            }
        }
        ExitCritical();
        regs->rax = 0;
    } else if (syscall_num == 52) { // sys_get_event
        // rdi: window_id, rsi: Event* (w userspace)
        int win_id = regs->rdi;
        uint64_t ev_ptr = regs->rsi;
        
        if (ev_ptr >= 0x400000) {
            for (int i = 0; i < 32; i++) {
                if (sys_windows[i].id == win_id && sys_windows[i].active) {
                    Window::Event e;
                    if (sys_windows[i].PopEvent(&e)) {
                        Window::Event* u_ev = (Window::Event*)ev_ptr;
                        *u_ev = e;
                        regs->rax = 1;
                    } else {
                        regs->rax = 0;
                    }
                    return;
                }
            }
        }
        regs->rax = 0;
    } else if (syscall_num == 57) { // sys_draw_bmp
        int win_id = regs->rdi;
        const char* path = (const char*)regs->rsi;
        int x = regs->rdx;
        int y = regs->r10;

        Window* target_win = nullptr;
        for (int i = 0; i < 32; i++) {
            if (sys_windows[i].id == win_id && sys_windows[i].active) {
                target_win = &sys_windows[i];
                break;
            }
        }
        
        if (target_win) {
            uint8_t* bmp_buf = nullptr;
            uint32_t bmp_size = 0;
            if (VFS::ReadFile(path, &bmp_buf, &bmp_size)) {
                BMP::DrawToBuffer(bmp_buf, target_win->fb_buffer, target_win->width - 4, target_win->height - 22, x, y);
                VFS::FreeFile(bmp_buf, bmp_size);
                regs->rax = 1;
            } else {
                regs->rax = 0;
            }
        } else {
            regs->rax = 0;
        }
    } else if (syscall_num == 55) { // sys_get_tasks
        TaskInfo* buffer = (TaskInfo*)regs->rdi;
        int max_count = regs->rsi;
        regs->rax = Scheduler::GetTasks(buffer, max_count);
    } else if (syscall_num == 56) { // sys_kill_task
        uint64_t task_id = regs->rdi;
        if (Scheduler::KillTaskById(task_id)) {
            MarkTaskWindowsForRemoval(task_id);
            regs->rax = 1;
        } else {
            regs->rax = 0;
        }
    } else {
        SerialPort::WriteString("Syscall: Unknown syscall number!\n");
    }
}
