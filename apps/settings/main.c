#include "gui.h"

int strcmp(const char* s1, const char* s2) {
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void draw_sidebar(uint32_t* fb, int win_w, int win_h, int state) {
    // Tło sidebaru
    gui_draw_rect(fb, win_w, 0, 0, 140, win_h, 0x2C3E50); // ciemnoniebieski

    // Kategoria: Wygląd
    uint32_t color_wyglad = (state == 0) ? 0x34495E : 0x2C3E50;
    gui_draw_rect(fb, win_w, 0, 40, 140, 40, color_wyglad);
    gui_draw_string(fb, win_w, "Wyglad", 20, 56, 0xFFFFFF, color_wyglad);

    // Kategoria: System
    uint32_t color_system = (state == 1) ? 0x34495E : 0x2C3E50;
    gui_draw_rect(fb, win_w, 0, 80, 140, 40, color_system);
    gui_draw_string(fb, win_w, "System", 20, 96, 0xFFFFFF, color_system);
}

void draw_home(uint32_t* fb, int win_w, int win_h, int win_id) {
    // Tło główne
    gui_draw_rect(fb, win_w, 140, 0, win_w - 140, win_h, 0xECF0F1); // jasnoszary
    gui_draw_string(fb, win_w, "Ustawienia - Wyglad", 160, 20, 0x2C3E50, 0xECF0F1);

    // Sekcja tapet
    gui_draw_string(fb, win_w, "Wybierz tapete:", 160, 60, 0x000000, 0xECF0F1);

    // Thumbnail 1 (Synthwave)
    if (!sys_draw_bmp(win_id, "/PICS/wp1_thumb.bmp", 160, 90)) {
        gui_draw_rect(fb, win_w, 160, 90, 160, 90, 0xBDC3C7);
        gui_draw_string(fb, win_w, "Brak wp1_thumb", 180, 130, 0x000000, 0xBDC3C7);
    }
    
    // Thumbnail 2 (Abstrakcja)
    if (!sys_draw_bmp(win_id, "/PICS/wp2_thumb.bmp", 340, 90)) {
        gui_draw_rect(fb, win_w, 340, 90, 160, 90, 0xBDC3C7);
        gui_draw_string(fb, win_w, "Brak wp2_thumb", 360, 130, 0x000000, 0xBDC3C7);
    }

    // Thumbnail 3 (Natura)
    if (!sys_draw_bmp(win_id, "/PICS/wp3_thumb.bmp", 160, 200)) {
        gui_draw_rect(fb, win_w, 160, 200, 160, 90, 0xBDC3C7);
        gui_draw_string(fb, win_w, "Brak wp3_thumb", 180, 240, 0x000000, 0xBDC3C7);
    }

    draw_sidebar(fb, win_w, win_h, 0);
    sys_update_window(win_id);
}

void draw_system(uint32_t* fb, int win_w, int win_h, int win_id) {
    gui_draw_rect(fb, win_w, 140, 0, win_w - 140, win_h, 0xECF0F1);
    gui_draw_string(fb, win_w, "Ustawienia - System", 160, 20, 0x2C3E50, 0xECF0F1);
    gui_draw_string(fb, win_w, "Wersja: OxideOS v1.1.0", 160, 60, 0x000000, 0xECF0F1);
    gui_draw_string(fb, win_w, "Jadro: 64-bit Long Mode", 160, 80, 0x000000, 0xECF0F1);
    gui_draw_string(fb, win_w, "Menedzer: Wlasny Compositor", 160, 100, 0x000000, 0xECF0F1);
    
    draw_sidebar(fb, win_w, win_h, 1);
    sys_update_window(win_id);
}

void _start() {
    uint32_t* fb = 0;
    int win_w = 540;
    int win_h = 360;
    int win_id = sys_create_window("Panel Sterowania", win_w, win_h, 100, 100, &fb);
    
    if (win_id < 0 || !fb) {
        sys_print("Failed to create Settings window\n");
        sys_exit();
    }
    
    int state = 0; // 0 = Wyglad, 1 = System
    
    draw_home(fb, win_w, win_h, win_id);
    
    struct WindowEvent ev;
    while (1) {
        if (sys_get_event(win_id, &ev)) {
            if (ev.type == 1) { // Mouse Click
                // Sidebar clicks
                if (ev.x >= 0 && ev.x < 140) {
                    if (ev.y >= 40 && ev.y < 80 && state != 0) {
                        state = 0;
                        draw_home(fb, win_w, win_h, win_id);
                    } else if (ev.y >= 80 && ev.y < 120 && state != 1) {
                        state = 1;
                        draw_system(fb, win_w, win_h, win_id);
                    }
                }
                
                // Content clicks
                if (state == 0) {
                    if (ev.y >= 90 && ev.y <= 180) {
                        if (ev.x >= 160 && ev.x <= 320) {
                            sys_write_file("/DOCS/CONFIG.DAT", (const uint8_t*)"/wp1.bmp", 8);
                            sys_reload_wallpaper();
                        } else if (ev.x >= 340 && ev.x <= 500) {
                            sys_write_file("/DOCS/CONFIG.DAT", (const uint8_t*)"/wp2.bmp", 8);
                            sys_reload_wallpaper();
                        }
                    } else if (ev.y >= 200 && ev.y <= 290) {
                        if (ev.x >= 160 && ev.x <= 320) {
                            sys_write_file("/DOCS/CONFIG.DAT", (const uint8_t*)"/wp3.bmp", 8);
                            sys_reload_wallpaper();
                        }
                    }
                }
            } else if (ev.type == 3) { // Close
                sys_exit();
            }
        }
        for (volatile int i = 0; i < 10000; i++);
    }
}
