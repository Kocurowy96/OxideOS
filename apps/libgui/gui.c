#include "gui.h"

void sys_print(const char* str) {
    long syscall_num = 1;
    asm volatile("int $0x80" : : "a"(syscall_num), "D"(str));
}

void sys_exit() {
    long syscall_num = 2;
    asm volatile("int $0x80" : : "a"(syscall_num));
}

int sys_get_tasks(struct TaskInfo* buffer, int max_count) {
    long syscall_num = 55;
    long ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(syscall_num), "D"(buffer), "S"(max_count));
    return ret;
}

int sys_kill_task(unsigned long long task_id) {
    long syscall_num = 56;
    long ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(syscall_num), "D"(task_id));
    return ret;
}

int sys_draw_bmp(int win_id, const char* path, int x, int y) {
    long syscall_num = 57;
    long ret;
    register long r10 asm("r10") = y;
    asm volatile("int $0x80" : "=a"(ret) : "a"(syscall_num), "D"(win_id), "S"(path), "d"(x), "r"(r10));
    return ret;
}

int sys_write_file(const char* path, const uint8_t* buf, uint32_t size) {
    long syscall_num = 3;
    long ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(syscall_num), "D"(path), "S"(buf), "d"((long)size));
    return (int)ret;
}

int sys_get_time(struct DateTime* dt) {
    long syscall_num = 4;
    long ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(syscall_num), "D"(dt));
    return (int)ret;
}

int sys_get_mem_info(uint64_t* total, uint64_t* free) {
    long syscall_num = 6;
    long ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(syscall_num), "D"(total), "S"(free));
    return (int)ret;
}

int sys_get_display_info(uint32_t* width, uint32_t* height, uint32_t* bpp) {
    long syscall_num = 7;
    long ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(syscall_num), "D"(width), "S"(height), "d"(bpp));
    return (int)ret;
}

int sys_play_wav(const char* path) {
    long syscall_num = 8;
    long ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(syscall_num), "D"(path));
    return (int)ret;
}

int sys_set_volume(uint32_t percent) {
    long syscall_num = 9;
    long ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(syscall_num), "D"((long)percent));
    return (int)ret;
}

int sys_get_volume(uint32_t* percent) {
    long syscall_num = 10;
    long ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(syscall_num), "D"(percent));
    return (int)ret;
}

void sys_reload_wallpaper() {
    long syscall_num = 53;
    long ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(syscall_num));
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
    long ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(syscall_num), "D"(window_id));
}

void sys_destroy_window(int window_id) {
    long syscall_num = 54;
    long ret;
    asm volatile("int $0x80" : "=a"(ret) : "a"(syscall_num), "D"(window_id));
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

#include "font8x8.h"

void gui_draw_string(uint32_t* fb, int win_w, const char* str, int x, int y, uint32_t fg, uint32_t bg) {
    int cx = x;
    while (*str) {
        if (*str == '\n') {
            cx = x;
            y += 8;
        } else {
            unsigned char c = (unsigned char)*str;
            if (c >= 0 && c <= 127) {
                const unsigned char* glyph = font8x8_basic[c];
                for (int cy = 0; cy < 8; cy++) {
                    for (int px = 0; px < 8; px++) {
                        if (glyph[cy] & (1 << px)) {
                            fb[(y + cy) * win_w + (cx + px)] = fg;
                        } else {
                            if (bg != 0xFFFFFFFF) {
                                fb[(y + cy) * win_w + (cx + px)] = bg;
                            }
                        }
                    }
                }
            }
            cx += 8;
        }
        str++;
    }
}
