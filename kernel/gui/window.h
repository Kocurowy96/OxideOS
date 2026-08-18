#pragma once
#include <stdint.h>

class Window {
public:
    Window(int start_x, int start_y, int width, int height, const char* title);
    
    int id;
    int x, y;
    int width, height;
    const char* title;
    
    // Zmienne stanu przeciągania
    bool is_dragging;
    int drag_start_x;
    int drag_start_y;
    int drag_start_win_x;
    int drag_start_win_y;
    
    class Application* app = nullptr;
    
    // Wsparcie dla Userspace GUI
    uint32_t* fb_buffer = nullptr;
    
    struct Event {
        int type; // 1 = MouseClick, 2 = KeyPress
        int x, y;
        char key;
    };
    
    Event events[16];
    int event_head = 0;
    int event_tail = 0;
    
    bool PushEvent(const Event& e) {
        int next = (event_head + 1) % 16;
        if (next == event_tail) return false; // Full
        events[event_head] = e;
        event_head = next;
        return true;
    }
    
    bool PopEvent(Event* e) {
        if (event_head == event_tail) return false; // Empty
        *e = events[event_tail];
        event_tail = (event_tail + 1) % 16;
        return true;
    }
};
