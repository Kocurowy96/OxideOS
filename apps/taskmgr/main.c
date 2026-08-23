#include <gui.h>
#include <stddef.h>
#include <stdbool.h>

int win_id;
uint32_t* fb;
int win_w = 400;
int win_h = 300;

struct TaskInfo tasks[16];
int task_count = 0;
int selected_task = -1;

void IntToString(int val, char* str) {
    if (val == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }
    int temp = val;
    int len = 0;
    while (temp > 0) {
        len++;
        temp /= 10;
    }
    str[len] = '\0';
    for (int i = len - 1; i >= 0; i--) {
        str[i] = (val % 10) + '0';
        val /= 10;
    }
}

void DrawAppButton(int x, int y, int w, int h, const char* text, bool pressed) {
    gui_draw_rect(fb, win_w, x, y, w, h, 0xC0C0C0);
    if (pressed) {
        gui_draw_rect(fb, win_w, x, y, w, 1, 0x000000);
        gui_draw_rect(fb, win_w, x, y, 1, h, 0x000000);
    } else {
        gui_draw_rect(fb, win_w, x, y, w, 1, 0xFFFFFF);
        gui_draw_rect(fb, win_w, x, y, 1, h, 0xFFFFFF);
        gui_draw_rect(fb, win_w, x + w - 1, y, 1, h, 0x000000);
        gui_draw_rect(fb, win_w, x, y + h - 1, w, 1, 0x000000);
    }
    
    int len = 0;
    while(text[len]) len++;
    int tx = x + (w - len * 8) / 2;
    int ty = y + (h - 8) / 2;
    gui_draw_string(fb, win_w, text, tx + (pressed ? 1 : 0), ty + (pressed ? 1 : 0), 0x000000, 0xC0C0C0);
}

void PaintTaskmgr() {
    gui_draw_rect(fb, win_w, 0, 0, win_w, win_h, 0xC0C0C0);
    gui_draw_string(fb, win_w, "Zadania:", 10, 10, 0x000000, 0xC0C0C0);
    
    // Lista zadan
    gui_draw_rect(fb, win_w, 10, 30, 380, 200, 0xFFFFFF);
    gui_draw_rect(fb, win_w, 9, 29, 382, 1, 0x000000); // top border
    gui_draw_rect(fb, win_w, 9, 29, 1, 202, 0x000000); // left border
    
    for (int i = 0; i < task_count; i++) {
        int y = 32 + (i * 16);
        uint32_t bg_color = (i == selected_task) ? 0x000080 : 0xFFFFFF;
        uint32_t fg_color = (i == selected_task) ? 0xFFFFFF : 0x000000;
        
        gui_draw_rect(fb, win_w, 10, y, 380, 16, bg_color);
        
        char pid_str[16];
        IntToString(tasks[i].id, pid_str);
        
        gui_draw_string(fb, win_w, pid_str, 12, y + 4, fg_color, bg_color);
        gui_draw_string(fb, win_w, tasks[i].name, 60, y + 4, fg_color, bg_color);
    }
    
    DrawAppButton(270, 250, 120, 30, "Zakoncz", false);
}

void OnMouseClick(int x, int y) {
    if (x >= 10 && x <= 390 && y >= 30 && y <= 230) {
        int idx = (y - 32) / 16;
        if (idx >= 0 && idx < task_count) {
            selected_task = idx;
        } else {
            selected_task = -1;
        }
    }
    
    if (x >= 270 && x <= 390 && y >= 250 && y <= 280) {
        if (selected_task >= 0 && selected_task < task_count) {
            sys_kill_task(tasks[selected_task].id);
            selected_task = -1;
            task_count = sys_get_tasks(tasks, 16);
        }
    }
}

void _start() {
    win_id = sys_create_window("Menedzer Zadan", win_w, win_h, 300, 200, &fb);
    if (win_id < 0 || !fb) sys_exit();
    
    task_count = sys_get_tasks(tasks, 16);
    PaintTaskmgr();
    sys_update_window(win_id);
    
    struct WindowEvent ev;
    int ticks = 0;
    while (1) {
        if (sys_get_event(win_id, &ev)) {
            if (ev.type == 1) { // Mouse click
                if (ev.x >= 270 && ev.x <= 390 && ev.y >= 250 && ev.y <= 280) {
                    DrawAppButton(270, 250, 120, 30, "Zakoncz", true);
                    sys_update_window(win_id);
                    for(volatile int j = 0; j < 5000000; j++); // mały delay
                }
                
                OnMouseClick(ev.x, ev.y);
                PaintTaskmgr();
                sys_update_window(win_id);
            } else if (ev.type == 3) { // Close
                sys_exit();
            }
        }
        
        ticks++;
        if (ticks > 50) { // odświeżanie
            ticks = 0;
            int new_count = sys_get_tasks(tasks, 16);
            if (new_count != task_count) { // Odśwież gdy liczba zadań się zmieni (uproszczone)
                task_count = new_count;
                if (selected_task >= task_count) selected_task = -1;
                PaintTaskmgr();
                sys_update_window(win_id);
            }
        }
        for (volatile int i = 0; i < 10000; i++); // cpu delay
    }
}
