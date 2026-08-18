#pragma once

#include "window.h"

#define MAX_WINDOWS 10

class Compositor {
public:
    static void Init();
    static void Render();
    
    // Zarządzanie oknami
    static bool AddWindow(Window* win);
    static void RemoveWindow(Window* win);
    
    static void HandleKeyPress(char c);
    
private:
    static Window* windows[MAX_WINDOWS];
    static int window_count;

    static void* icon_bmp;

    // Zmienne stanu GUI
    static bool prev_mouse_left;
    static bool start_menu_open;
};
