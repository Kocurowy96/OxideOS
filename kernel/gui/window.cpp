#include "window.h"

static int next_window_id = 1;

Window::Window(int start_x, int start_y, int w, int h, const char* t) {
    id = next_window_id++;
    x = start_x;
    y = start_y;
    width = w;
    height = h;
    title = t;
    
    is_dragging = false;
    is_minimized = false;
    app = nullptr;
    fb_buffer = nullptr;
    event_head = 0;
    event_tail = 0;
    drag_start_x = 0;
    drag_start_y = 0;
    drag_start_win_x = 0;
    drag_start_win_y = 0;
}
