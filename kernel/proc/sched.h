#pragma once
#include <stdint.h>
#include "../cpu/isr.h"

struct Task {
    uint64_t id;
    Registers regs;
    bool active;
};

class Scheduler {
public:
    static void Init();
    static void CreateTask(void (*entry)());
    static Registers* Schedule(Registers* regs);
    static void KillCurrentTask();
};

extern "C" void SwitchToUserMode(uint64_t entry, uint64_t stack);
