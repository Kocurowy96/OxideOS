#pragma once
#include "application.h"

class WelcomeApp : public Application {
public:
    void OnInit(Window* win) override;
    void OnPaint(int win_x, int win_y, int width, int height) override;
};

