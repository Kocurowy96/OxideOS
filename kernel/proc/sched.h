#pragma once
#include <stdint.h>
#include "../cpu/isr.h"

struct Task {
    uint64_t id;
    Registers regs;
    bool active;
    char name[32];
    uint64_t kernel_stack_top; // Wlasny stos jadra (TSS.rsp0) na przejscia Ring3->Ring0
};

struct TaskInfo {
    uint64_t id;
    char name[32];
};

class Scheduler {
public:
    static void Init();
    static void CreateTask(void (*entry)(void*), void* arg);
    static Registers* Schedule(Registers* regs);
    static void KillCurrentTask();
    
    // Nowe funkcje dla Taskmgr
    static int GetTasks(TaskInfo* buffer, int max_count);
    static bool KillTaskById(uint64_t id);
    static uint64_t GetCurrentTaskId();
};

extern "C" void SwitchToUserMode(uint64_t entry, uint64_t stack);
