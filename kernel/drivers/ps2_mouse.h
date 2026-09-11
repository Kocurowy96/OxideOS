#pragma once
#include <stdint.h>

class Mouse {
public:
    static void Init();
    static void HandleInterrupt();
    static void SetSpeed(uint32_t percent); // 25-300, klamrowane; 100 = normalna czulosc (1:1)
    static uint32_t GetSpeed();
};

extern int32_t mouse_x;
extern int32_t mouse_y;
extern bool mouse_left;
extern bool mouse_right;
extern bool mouse_middle;
