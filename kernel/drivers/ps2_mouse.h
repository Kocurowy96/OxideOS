#pragma once
#include <stdint.h>

class Mouse {
public:
    static void Init();
    static void HandleInterrupt();
};

extern int32_t mouse_x;
extern int32_t mouse_y;
extern bool mouse_left;
extern bool mouse_right;
extern bool mouse_middle;
