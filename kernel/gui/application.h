#pragma once
#include <stdint.h>

class Window;

class Application {
public:
    virtual ~Application() {}
    
    // Zdarzenia cyklu życia
    virtual void OnInit(Window* win) {}
    virtual void OnClose() {}
    
    // Zdarzenia UI
    // win_x, win_y to absolutne koordynaty lewego górnego rogu obszaru roboczego okna
    virtual void OnPaint(int win_x, int win_y, int width, int height) {}
    
    // local_x, local_y to koordynaty relatywne do obszaru roboczego okna
    virtual void OnMouseClick(int local_x, int local_y) {}
    virtual void OnMouseRelease(int local_x, int local_y) {}
    virtual void OnMouseMove(int local_x, int local_y) {}
    
    virtual void OnKeyPress(char c) {}
    
    // Opcjonalny dostęp do okna
    Window* window = nullptr;
};
