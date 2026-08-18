#pragma once
#include "application.h"

class CalculatorApp : public Application {
public:
    void OnInit(Window* win) override;
    void OnPaint(int win_x, int win_y, int width, int height) override;
    void OnMouseClick(int local_x, int local_y) override;
    
private:
    void PlayErrorSound();
    
    char display[32];
    int current_val;
    int stored_val;
    char current_op;
    bool new_number;
};

class NotepadApp : public Application {
public:
    void OnInit(Window* win) override;
    void OnPaint(int win_x, int win_y, int width, int height) override;
    void OnKeyPress(char c) override;
    
private:
    char text_buffer[1024];
    int cursor_pos;
};
