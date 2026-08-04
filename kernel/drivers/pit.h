#pragma once
#include <stdint.h>

class PIT {
public:
    static void Init(uint32_t frequency);
    static void Tick();
    static uint64_t GetTicks();
};
