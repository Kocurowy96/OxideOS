#include "gui.h"

void _start() {
    uint32_t* fb = 0;
    int win_w = 400;
    int win_h = 200;
    int win_id = sys_create_window("Witaj w OxideOS!", win_w, win_h, 312, 284, &fb);
    
    if (win_id < 0 || !fb) {
        sys_print("Failed to create Welcome window\n");
        sys_exit();
    }
    
    gui_draw_rect(fb, win_w, 0, 0, win_w, win_h, 0xC0C0C0);
    
    gui_draw_string(fb, win_w, "Witaj w OxideOS!", 140, 20, 0x000080, 0xC0C0C0);
    gui_draw_string(fb, win_w, "System zostal pomyslnie zaktualizowany.", 20, 60, 0x000000, 0xC0C0C0);
    gui_draw_string(fb, win_w, "Wersja: 1.0.0 (Przejscie na Userspace)", 20, 80, 0x000000, 0xC0C0C0);
    gui_draw_string(fb, win_w, "Zyczymy milego korzystania!", 20, 120, 0x000000, 0xC0C0C0);
    
    // Narysuj przycisk "OK"
    int btn_w = 100;
    int btn_h = 30;
    int btn_x = (win_w - btn_w) / 2;
    int btn_y = win_h - 40;
    
    gui_draw_rect(fb, win_w, btn_x, btn_y, btn_w, btn_h, 0xC0C0C0);
    gui_draw_rect(fb, win_w, btn_x, btn_y, btn_w, 1, 0xFFFFFF); // top
    gui_draw_rect(fb, win_w, btn_x, btn_y, 1, btn_h, 0xFFFFFF); // left
    gui_draw_rect(fb, win_w, btn_x + btn_w - 1, btn_y, 1, btn_h, 0x000000); // right
    gui_draw_rect(fb, win_w, btn_x, btn_y + btn_h - 1, btn_w, 1, 0x000000); // bottom
    
    gui_draw_string(fb, win_w, "OK", btn_x + 42, btn_y + 11, 0x000000, 0xC0C0C0);
    
    sys_update_window(win_id);
    
    struct WindowEvent ev;
    while (1) {
        if (sys_get_event(win_id, &ev)) {
            if (ev.type == 1) { // Mouse Click
                if (ev.x >= btn_x && ev.x <= btn_x + btn_w && ev.y >= btn_y && ev.y <= btn_y + btn_h) {
                    sys_exit(); // Zamknij okno i aplikację
                }
            } else if (ev.type == 3) {
                sys_exit();
            }
        }
        for (volatile int i = 0; i < 10000; i++);
    }
}
