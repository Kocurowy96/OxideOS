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

    // Dla panic handlera (isr.cpp, int_no==14): oczekiwany gorny adres stosu jadra
    // biezacego taska, zeby porownac z tym co faktycznie jest w TSS.rsp0 (GDT::GetTSSStack)
    // - rozni sie, to synchronizacja TSS.rsp0 jest zepsuta, nie przepelnienie stosu.
    static uint64_t GetCurrentKernelStackTop();
};

extern "C" void SwitchToUserMode(uint64_t entry, uint64_t stack);
