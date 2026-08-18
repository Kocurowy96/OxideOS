#include "window.h"

Window::Window(int start_x, int start_y, int w, int h, const char* t) {
    x = start_x;
    y = start_y;
    width = w;
    height = h;
    title = t;
    
    is_dragging = false;
    drag_start_x = 0;
    drag_start_y = 0;
    drag_start_win_x = 0;
    drag_start_win_y = 0;
}
