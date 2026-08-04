#pragma once
#include <stdint.h>

class RTC {
public:
    static void PrintTime();
private:
    static uint8_t ReadRegister(uint8_t reg);
};
