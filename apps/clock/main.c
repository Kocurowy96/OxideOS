#include "gui.h"
#include "font8x8.h"

static void draw_digit(uint32_t* fb, int win_w, char digit, int x, int y, uint32_t color) {
    // Prosta symulacja wiekszej czcionki, np. skalujemy znak 8x8 x3 = 24x24
    const unsigned char* glyph = font8x8_basic[(int)digit];
    
    for (int cy = 0; cy < 8; cy++) {
        for (int px = 0; px < 8; px++) {
            if (glyph[cy] & (1 << px)) {
                gui_draw_rect(fb, win_w, x + (px * 3), y + (cy * 3), 3, 3, color);
            }
        }
    }
}

static void draw_large_string(uint32_t* fb, int win_w, const char* str, int x, int y, uint32_t color) {
    int cx = x;
    while (*str) {
        draw_digit(fb, win_w, *str, cx, y, color);
        cx += 24; // Znak x3
        str++;
    }
}

void _start() {
    uint32_t* fb = 0;
    int win_w = 260;
    int win_h = 120;
    int win_id = sys_create_window("Zegar Systemowy", win_w, win_h, 800, 100, &fb);
    
    if (win_id < 0 || !fb) {
        sys_print("Failed to create Clock window\n");
        sys_exit();
    }
    
    uint8_t last_s = 255;
    
    struct WindowEvent ev;
    while (1) {
        struct DateTime dt;
        sys_get_time(&dt);
        
        // Zoptymalizowane - odswiezamy tylko jak sekundnik przeskoczy (albo minuty dla zegarka bez sekundnika)
        if (dt.minute != last_s) {
            last_s = dt.minute;
            
            gui_draw_rect(fb, win_w, 0, 0, win_w, win_h, 0xC0C0C0);
            
            // "Ekranik" na godzine
            gui_draw_rect(fb, win_w, 20, 20, 220, 50, 0x000000);
            gui_draw_rect(fb, win_w, 20, 20, 220, 1, 0x808080);
            gui_draw_rect(fb, win_w, 20, 20, 1, 50, 0x808080);
            gui_draw_rect(fb, win_w, 239, 20, 1, 50, 0xFFFFFF);
            gui_draw_rect(fb, win_w, 20, 69, 220, 1, 0xFFFFFF);
            
            char time_str[6] = {
                (char)('0' + (dt.hour / 10)), (char)('0' + (dt.hour % 10)), ':',
                (char)('0' + (dt.minute / 10)), (char)('0' + (dt.minute % 10)), '\0'
            };
            
            // Rysujemy "LED" clock
            draw_large_string(fb, win_w, time_str, 70, 32, 0x00FF00); // Zielony
            
            // Data
            char date_str[11] = {
                (char)('0' + (dt.day / 10)), (char)('0' + (dt.day % 10)), '.',
                (char)('0' + (dt.month / 10)), (char)('0' + (dt.month % 10)), '.',
                '2', '0', 
                (char)('0' + (dt.year / 10)), (char)('0' + (dt.year % 10)), '\0'
            };
            gui_draw_string(fb, win_w, date_str, 95, 85, 0x000000, 0xC0C0C0);
            
            sys_update_window(win_id);
        }
        
        while (sys_get_event(win_id, &ev)) {
            if (ev.type == 1) { 
                // Nic nie rób, nie zamykamy po kliknięciu
            } else if (ev.type == 3) {
                sys_exit();
            }
        }
        
        for (volatile int i = 0; i < 10000; i++);
    }
}
