#include "gui.h"

static void itoa(uint64_t val, char* buf) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    int pos = 0;
    char rev[32];
    while (val > 0) {
        rev[pos++] = (val % 10) + '0';
        val /= 10;
    }
    int dpos = 0;
    while (pos > 0) {
        buf[dpos++] = rev[--pos];
    }
    buf[dpos] = '\0';
}

static int strlen(const char* s) {
    int len = 0;
    while(s[len]) len++;
    return len;
}

static void strcpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

static void strcat(char* dest, const char* src) {
    while (*dest) dest++;
    while (*src) *dest++ = *src++;
    *dest = '\0';
}

void _start() {
    uint32_t* fb = 0;
    int win_w = 460;
    int win_h = 320;
    int win_id = sys_create_window("O Systemie (WINVER)", win_w, win_h, 440, 240, &fb);
    
    if (win_id < 0 || !fb) {
        sys_print("Failed to create Winver window\n");
        sys_exit();
    }
    
    gui_draw_rect(fb, win_w, 0, 0, win_w, win_h, 0xC0C0C0);
    
    // Narysujemy logo (winver.bmp)
    // Szerokosc baneru to 400px. Okno ma 460px. (460 - 400) / 2 = 30.
    sys_draw_bmp(win_id, "/winver.bmp", 30, 15);
    
    gui_draw_string(fb, win_w, "System operacyjny OxideOS", 30, 180, 0x000000, 0xC0C0C0);
    gui_draw_string(fb, win_w, "Stworzony z pomoca Antigravity", 30, 195, 0x000000, 0xC0C0C0);
    
    uint64_t total_mem = 0;
    uint64_t free_mem = 0;
    sys_get_mem_info(&total_mem, &free_mem);
    
    char mem_str[100];
    char num_buf[32];
    
    strcpy(mem_str, "Pamiec fizyczna RAM: ");
    itoa(total_mem / (1024 * 1024), num_buf);
    strcat(mem_str, num_buf);
    strcat(mem_str, " MB (Wolne: ");
    itoa(free_mem / (1024 * 1024), num_buf);
    strcat(mem_str, num_buf);
    strcat(mem_str, " MB)");
    
    gui_draw_string(fb, win_w, mem_str, 30, 225, 0x000000, 0xC0C0C0);
    
    // Narysuj przycisk "OK"
    int btn_w = 80;
    int btn_h = 24;
    int btn_x = (win_w - btn_w) / 2;
    int btn_y = win_h - 40;
    
    gui_draw_rect(fb, win_w, btn_x, btn_y, btn_w, btn_h, 0xC0C0C0);
    gui_draw_rect(fb, win_w, btn_x, btn_y, btn_w, 1, 0xFFFFFF); // top
    gui_draw_rect(fb, win_w, btn_x, btn_y, 1, btn_h, 0xFFFFFF); // left
    gui_draw_rect(fb, win_w, btn_x + btn_w - 1, btn_y, 1, btn_h, 0x000000); // right
    gui_draw_rect(fb, win_w, btn_x, btn_y + btn_h - 1, btn_w, 1, 0x000000); // bottom
    
    gui_draw_string(fb, win_w, "OK", btn_x + 32, btn_y + 8, 0x000000, 0xC0C0C0);
    
    sys_update_window(win_id);
    
    struct WindowEvent ev;
    while (1) {
        if (sys_get_event(win_id, &ev)) {
            if (ev.type == 1) { // Mouse Click
                if (ev.x >= btn_x && ev.x <= btn_x + btn_w && ev.y >= btn_y && ev.y <= btn_y + btn_h) {
                    sys_exit();
                }
            } else if (ev.type == 3) { // Close
                sys_exit();
            }
        }
        for (volatile int i = 0; i < 10000; i++);
    }
}
