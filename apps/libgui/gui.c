#include "gui.h"

void sys_print(const char* str) {
    long syscall_num = 1;
    asm volatile("int $0x80" : : "a"(syscall_num), "D"(str));
}

void sys_exit() {
    long syscall_num = 2;
    asm volatile("int $0x80" : : "a"(syscall_num));
}

int sys_write_file(const char* path, const uint8_t* buf, uint32_t size) {
    int res;
    asm volatile(
        "mov $3, %%rax\n"
        "mov %1, %%rdi\n"
        "mov %2, %%rsi\n"
        "mov %3, %%rdx\n"
        "syscall\n"
        "mov %%eax, %0\n"
        : "=r"(res) : "r"(path), "r"(buf), "r"((uint64_t)size) : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory"
    );
    return res;
}

int sys_get_time(struct DateTime* dt) {
    int res;
    asm volatile(
        "mov $4, %%rax\n"
        "mov %1, %%rdi\n"
        "syscall\n"
        "mov %%eax, %0\n"
        : "=r"(res) : "r"(dt) : "rax", "rdi", "rcx", "r11", "memory"
    );
    return res;
}

int sys_create_window(const char* title, int width, int height, int x, int y, uint32_t** fb_buffer) {
    long syscall_num = 50;
    long win_id;
    
    struct {
        int id;
        uint64_t fb; // 64-bit pointer
    } res;
    
    register long r10 asm("r10") = x;
    register long r8 asm("r8") = y;
    register long r9 asm("r9") = (long)&res;
    
    asm volatile("int $0x80" 
        : "=a"(win_id) 
        : "a"(syscall_num), "D"(title), "S"(width), "d"(height), "r"(r10), "r"(r8), "r"(r9)
        : "memory"
    );
    
    if (fb_buffer) {
        *fb_buffer = (uint32_t*)res.fb;
    }
    return win_id;
}

void sys_update_window(int window_id) {
    long syscall_num = 51;
    asm volatile("int $0x80" : : "a"(syscall_num), "D"((long)window_id));
}

int sys_get_event(int window_id, struct WindowEvent* ev) {
    long syscall_num = 52;
    long ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(syscall_num), "D"((long)window_id), "S"(ev) : "memory");
    return (int)ret;
}

void gui_draw_rect(uint32_t* fb, int win_w, int x, int y, int w, int h, uint32_t color) {
    for (int r = 0; r < h; r++) {
        for (int c = 0; c < w; c++) {
            if (x + c >= 0 && x + c < win_w) {
                fb[(y + r) * win_w + (x + c)] = color;
            }
        }
    }
}
