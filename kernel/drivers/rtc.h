#pragma once
#include <stdint.h>

class RTC {
public:
    static void PrintTime();
    static uint8_t GetHour();
    static uint8_t GetMinute();
private:
    static uint8_t ReadRegister(uint8_t reg);
};
