#pragma once
#include <stdint.h>

struct WindowEvent {
    int type; // 1 = MouseClick, 2 = KeyPress
    int x, y;
    char key;
};

int sys_create_window(const char* title, int width, int height, int x, int y, uint32_t** fb_buffer);
void sys_update_window(int window_id);
int sys_get_event(int window_id, struct WindowEvent* ev);
void sys_print(const char* str);
void sys_exit();
int sys_write_file(const char* path, const uint8_t* buffer, uint32_t size);

void gui_draw_rect(uint32_t* fb, int win_w, int x, int y, int w, int h, uint32_t color);
