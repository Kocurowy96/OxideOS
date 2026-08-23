#include <gui.h>
#include <stddef.h>
#include <stdbool.h>

int win_id;
uint32_t* fb;
int win_w = 500;
int win_h = 350;

char text_buffer[4096];
int text_len = 0;

void PaintNotepad() {
    // Menu bar / Background
    gui_draw_rect(fb, win_w, 0, 0, win_w, win_h, 0xC0C0C0);
    
    // Menu (dummy)
    gui_draw_string(fb, win_w, "Plik", 10, 5, 0x000000, 0xC0C0C0);
    gui_draw_string(fb, win_w, "Edycja", 50, 5, 0x000000, 0xC0C0C0);
    gui_draw_string(fb, win_w, "Pomoc", 110, 5, 0x000000, 0xC0C0C0);
    
    // Text area
    gui_draw_rect(fb, win_w, 2, 20, win_w - 4, win_h - 22, 0xFFFFFF);
    gui_draw_rect(fb, win_w, 1, 19, win_w - 2, 1, 0x000000); // top inner
    gui_draw_rect(fb, win_w, 1, 19, 1, win_h - 20, 0x000000); // left inner
    
    // Draw text with word wrap/newlines
    int cx = 5;
    int cy = 25;
    for (int i = 0; i < text_len; i++) {
        char c = text_buffer[i];
        if (c == '\n') {
            cx = 5;
            cy += 12;
            continue;
        }
        
        if (cx > win_w - 15) {
            cx = 5;
            cy += 12;
        }
        
        char buf[2] = {c, '\0'};
        gui_draw_string(fb, win_w, buf, cx, cy, 0x000000, 0xFFFFFF);
        cx += 8;
    }
    
    // Cursor
    gui_draw_rect(fb, win_w, cx, cy + 2, 8, 2, 0x000000);
}

void _start() {
    win_id = sys_create_window("Bez tytulu - OxidePad", win_w, win_h, 50, 50, &fb);
    if (win_id < 0 || !fb) sys_exit();
    
    text_buffer[0] = '\0';
    
    PaintNotepad();
    sys_update_window(win_id);
    
    struct WindowEvent ev;
    while (1) {
        if (sys_get_event(win_id, &ev)) {
            if (ev.type == 2) { // KeyPress
                char c = ev.key;
                if (c == '\b') {
                    if (text_len > 0) {
                        text_len--;
                        text_buffer[text_len] = '\0';
                    }
                } else {
                    if (text_len < 4095) {
                        text_buffer[text_len] = c;
                        text_len++;
                        text_buffer[text_len] = '\0';
                    }
                }
                PaintNotepad();
                sys_update_window(win_id);
            }
        }
        for (volatile int i = 0; i < 10000; i++);
    }
}
