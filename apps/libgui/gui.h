#pragma once
#include <stdint.h>

#define GUI_EVENT_MOUSE_CLICK 1
#define GUI_EVENT_KEY_PRESS 2
#define GUI_EVENT_CLOSE 3

struct WindowEvent {
    int type; // 1 = MouseClick, 2 = KeyPress, 3 = Close
    int x, y;
    char key;
};

int sys_create_window(const char* title, int w, int h, int x, int y, uint32_t** fb);
void sys_update_window(int win_id);
void sys_destroy_window(int win_id);
int sys_get_event(int window_id, struct WindowEvent* ev);
void sys_print(const char* str);
void sys_reload_wallpaper();

struct TaskInfo {
    unsigned long long id;
    char name[32];
};

int sys_get_tasks(struct TaskInfo* buffer, int max_count);
int sys_kill_task(unsigned long long task_id);
int sys_draw_bmp(int win_id, const char* path, int x, int y);
void sys_exit();
int sys_write_file(const char* path, const uint8_t* buffer, uint32_t size);

struct DateTime {
    uint8_t year, month, day, hour, minute, second;
};
int sys_get_time(struct DateTime* dt);
int sys_get_mem_info(uint64_t* total, uint64_t* free);
void sys_reload_wallpaper();

void gui_draw_rect(uint32_t* fb, int win_w, int x, int y, int w, int h, uint32_t color);
void gui_draw_string(uint32_t* fb, int win_w, const char* str, int x, int y, uint32_t fg, uint32_t bg);
