#include "gui.h"

int strcmp(const char* s1, const char* s2) {
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strlen(const char* s) {
    int len = 0;
    while(s[len]) len++;
    return len;
}

void draw_button(uint32_t* fb, int win_w, int x, int y, int w, int h, uint32_t color) {
    gui_draw_rect(fb, win_w, x, y, w, h, color);
    gui_draw_rect(fb, win_w, x, y, w, 1, 0xFFFFFF); // top
    gui_draw_rect(fb, win_w, x, y, 1, h, 0xFFFFFF); // left
    gui_draw_rect(fb, win_w, x + w - 1, y, 1, h, 0x000000); // right
    gui_draw_rect(fb, win_w, x, y + h - 1, w, 1, 0x000000); // bottom
}

void _start() {
    uint32_t* fb = 0;
    int win_w = 300;
    int win_h = 200;
    int win_id = sys_create_window("Ustawienia", win_w, win_h, 150, 150, &fb);
    
    if (win_id < 0 || !fb) {
        sys_print("Failed to create Settings window\n");
        sys_exit();
    }
    
    // Draw background
    gui_draw_rect(fb, win_w, 0, 0, win_w, win_h, 0xC0C0C0);
    
    // Draw buttons
    draw_button(fb, win_w, 20, 20, 260, 40, 0xC0C0C0); // btn 1
    draw_button(fb, win_w, 20, 80, 260, 40, 0xC0C0C0); // btn 2
    
    // Blue and Green boxes to identify them (since no font is available yet)
    gui_draw_rect(fb, win_w, 22, 22, 256, 36, 0x0000FF); // Blue button -> bg1
    gui_draw_rect(fb, win_w, 22, 82, 256, 36, 0x00FF00); // Green button -> bg2
    
    struct WindowEvent ev;
    while (1) {
        if (sys_get_event(win_id, &ev)) {
            if (ev.type == 1) { // Mouse Click
                if (ev.x >= 20 && ev.x <= 280) {
                    if (ev.y >= 20 && ev.y <= 60) {
                        sys_print("Clicked bg1!\n");
                        sys_write_file("/DOCS/CONFIG.DAT", (const uint8_t*)"/bg1.bmp", 8);
                    } else if (ev.y >= 80 && ev.y <= 120) {
                        sys_print("Clicked bg2!\n");
                        sys_write_file("/DOCS/CONFIG.DAT", (const uint8_t*)"/bg2.bmp", 8);
                    }
                }
            }
        }
        
        // Polling delay
        for (volatile int i = 0; i < 10000; i++);
    }
}
