#pragma once
#include <stdint.h>

class Window {
public:
    Window(int start_x, int start_y, int width, int height, const char* title);
    
    int x, y;
    int width, height;
    const char* title;
    
    // Zmienne stanu przeciągania
    bool is_dragging;
    int drag_start_x;
    int drag_start_y;
    int drag_start_win_x;
    int drag_start_win_y;
};
