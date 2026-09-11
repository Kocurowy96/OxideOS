#include "gui.h"

int strcmp(const char* s1, const char* s2) {
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

static void itoa(uint32_t val, char* buf) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    int pos = 0;
    char rev[16];
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

static void strcpy_(char* dest, const char* src) {
    while (*src) { *dest++ = *src++; }
    *dest = '\0';
}

static void strcat_(char* dest, const char* src) {
    while (*dest) dest++;
    while (*src) { *dest++ = *src++; }
    *dest = '\0';
}

static void draw_win_button(uint32_t* fb, int win_w, int x, int y, int w, int h, const char* text) {
    gui_draw_rect(fb, win_w, x, y, w, h, 0xC0C0C0);
    gui_draw_rect(fb, win_w, x, y, w, 1, 0xFFFFFF); // top
    gui_draw_rect(fb, win_w, x, y, 1, h, 0xFFFFFF); // left
    gui_draw_rect(fb, win_w, x + w - 1, y, 1, h, 0x000000); // right
    gui_draw_rect(fb, win_w, x, y + h - 1, w, 1, 0x000000); // bottom

    int text_len = 0;
    while (text[text_len]) text_len++;
    int text_x = x + (w - text_len * 8) / 2;
    int text_y = y + (h - 8) / 2;
    gui_draw_string(fb, win_w, text, text_x, text_y, 0x000000, 0xC0C0C0);
}

// Kolejnosc zakladek: 0 = Wyglad, 1 = Wyswietlacz, 2 = System, 3 = Dzwiek
void draw_sidebar(uint32_t* fb, int win_w, int win_h, int state) {
    // Tło sidebaru
    gui_draw_rect(fb, win_w, 0, 0, 140, win_h, 0x2C3E50); // ciemnoniebieski

    // Kategoria: Wygląd
    uint32_t color_wyglad = (state == 0) ? 0x34495E : 0x2C3E50;
    gui_draw_rect(fb, win_w, 0, 40, 140, 40, color_wyglad);
    gui_draw_string(fb, win_w, "Wyglad", 20, 56, 0xFFFFFF, color_wyglad);

    // Kategoria: Wyświetlacz
    uint32_t color_wyswietlacz = (state == 1) ? 0x34495E : 0x2C3E50;
    gui_draw_rect(fb, win_w, 0, 80, 140, 40, color_wyswietlacz);
    gui_draw_string(fb, win_w, "Wyswietlacz", 20, 96, 0xFFFFFF, color_wyswietlacz);

    // Kategoria: System
    uint32_t color_system = (state == 2) ? 0x34495E : 0x2C3E50;
    gui_draw_rect(fb, win_w, 0, 120, 140, 40, color_system);
    gui_draw_string(fb, win_w, "System", 20, 136, 0xFFFFFF, color_system);

    // Kategoria: Dźwięk
    uint32_t color_dzwiek = (state == 3) ? 0x34495E : 0x2C3E50;
    gui_draw_rect(fb, win_w, 0, 160, 140, 40, color_dzwiek);
    gui_draw_string(fb, win_w, "Dzwiek", 20, 176, 0xFFFFFF, color_dzwiek);
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

void draw_display(uint32_t* fb, int win_w, int win_h, int win_id) {
    gui_draw_rect(fb, win_w, 140, 0, win_w - 140, win_h, 0xECF0F1);
    gui_draw_string(fb, win_w, "Ustawienia - Wyswietlacz", 160, 20, 0x2C3E50, 0xECF0F1);

    uint32_t width = 0, height = 0, bpp = 0;
    sys_get_display_info(&width, &height, &bpp);

    char line[64];
    char num[16];

    strcpy_(line, "Rozdzielczosc: ");
    itoa(width, num); strcat_(line, num);
    strcat_(line, " x ");
    itoa(height, num); strcat_(line, num);
    gui_draw_string(fb, win_w, line, 160, 60, 0x000000, 0xECF0F1);

    strcpy_(line, "Glebia koloru: ");
    itoa(bpp, num); strcat_(line, num);
    strcat_(line, " bit");
    gui_draw_string(fb, win_w, line, 160, 80, 0x000000, 0xECF0F1);

    gui_draw_string(fb, win_w, "Zrodlo: Limine Framebuffer (GOP/VESA)", 160, 100, 0x000000, 0xECF0F1);
    gui_draw_string(fb, win_w, "Zmiana rozdzielczosci: niedostepna", 160, 120, 0x000000, 0xECF0F1);

    draw_sidebar(fb, win_w, win_h, 1);
    sys_update_window(win_id);
}

void draw_system(uint32_t* fb, int win_w, int win_h, int win_id) {
    gui_draw_rect(fb, win_w, 140, 0, win_w - 140, win_h, 0xECF0F1);
    gui_draw_string(fb, win_w, "Ustawienia - System", 160, 20, 0x2C3E50, 0xECF0F1);
    gui_draw_string(fb, win_w, "Wersja: OxideOS v1.1.0", 160, 60, 0x000000, 0xECF0F1);
    gui_draw_string(fb, win_w, "Jadro: 64-bit Long Mode", 160, 80, 0x000000, 0xECF0F1);
    gui_draw_string(fb, win_w, "Menedzer: Wlasny Compositor", 160, 100, 0x000000, 0xECF0F1);

    uint64_t total_mem = 0, free_mem = 0;
    sys_get_mem_info(&total_mem, &free_mem);

    char line[64];
    char num[16];
    strcpy_(line, "Pamiec RAM: ");
    itoa((uint32_t)(total_mem / (1024 * 1024)), num); strcat_(line, num);
    strcat_(line, " MB (Wolne: ");
    itoa((uint32_t)(free_mem / (1024 * 1024)), num); strcat_(line, num);
    strcat_(line, " MB)");
    gui_draw_string(fb, win_w, line, 160, 120, 0x000000, 0xECF0F1);

    draw_sidebar(fb, win_w, win_h, 2);
    sys_update_window(win_id);
}

void draw_sound(uint32_t* fb, int win_w, int win_h, int win_id, uint32_t volume) {
    gui_draw_rect(fb, win_w, 140, 0, win_w - 140, win_h, 0xECF0F1);
    gui_draw_string(fb, win_w, "Ustawienia - Dzwiek", 160, 20, 0x2C3E50, 0xECF0F1);

    char line[32];
    char num[16];
    strcpy_(line, "Glosnosc: ");
    itoa(volume, num); strcat_(line, num);
    strcat_(line, "%");
    gui_draw_string(fb, win_w, line, 160, 60, 0x000000, 0xECF0F1);

    draw_win_button(fb, win_w, 160, 85, 40, 30, "-");
    draw_win_button(fb, win_w, 210, 85, 40, 30, "+");
    draw_win_button(fb, win_w, 160, 135, 160, 30, "Testuj dzwiek");

    draw_sidebar(fb, win_w, win_h, 3);
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

    int state = 0; // 0 = Wyglad, 1 = Wyswietlacz, 2 = System, 3 = Dzwiek
    uint32_t volume = 80;
    sys_get_volume(&volume);

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
                        draw_display(fb, win_w, win_h, win_id);
                    } else if (ev.y >= 120 && ev.y < 160 && state != 2) {
                        state = 2;
                        draw_system(fb, win_w, win_h, win_id);
                    } else if (ev.y >= 160 && ev.y < 200 && state != 3) {
                        state = 3;
                        draw_sound(fb, win_w, win_h, win_id, volume);
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
                } else if (state == 3) {
                    if (ev.y >= 85 && ev.y <= 115) {
                        if (ev.x >= 160 && ev.x <= 200 && volume >= 10) { // "-"
                            volume -= 10;
                            sys_set_volume(volume);
                            draw_sound(fb, win_w, win_h, win_id, volume);
                        } else if (ev.x >= 210 && ev.x <= 250 && volume <= 90) { // "+"
                            volume += 10;
                            sys_set_volume(volume);
                            draw_sound(fb, win_w, win_h, win_id, volume);
                        }
                    } else if (ev.y >= 135 && ev.y <= 165 && ev.x >= 160 && ev.x <= 320) { // Testuj dzwiek
                        sys_play_wav("/notify.wav");
                    }
                }
            } else if (ev.type == 3) { // Close
                sys_exit();
            }
        }
        for (volatile int i = 0; i < 10000; i++);
    }
}
