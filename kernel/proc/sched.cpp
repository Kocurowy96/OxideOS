#include "sched.h"
#include "../mem/pmm.h"
#include "../mem/vmm.h"
#include "../serial.h"
#include "../limine.h"

extern volatile struct limine_hhdm_request hhdm_request;

#define MAX_TASKS 16

static Task tasks[MAX_TASKS];
static int current_task = -1;
static int task_count = 0;
static uint64_t next_id = 1;

void Scheduler::Init() {
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].active = false;
    }
}

void Scheduler::CreateTask(void (*entry)(void*), void* arg) {
    if (task_count >= MAX_TASKS) return;
    
    // Find empty slot
    int slot = -1;
    for(int i = 0; i < MAX_TASKS; i++) {
        if(!tasks[i].active) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return;
    
    Task* t = &tasks[slot];
    t->id = next_id++;
    t->active = true;
    
    // Allocate stack (4 pages = 16384 bytes)
    void* stack = PMM::AllocatePages(4);
    uint64_t stack_top = (uint64_t)stack + 16384;
    if (hhdm_request.response != nullptr) {
        stack_top += hhdm_request.response->offset;
    }
    
    // Zero out registers
    for(size_t i = 0; i < sizeof(Registers); i++) {
        ((uint8_t*)&t->regs)[i] = 0;
    }
    
    // Zapisz nazwę (arg to ścieżka do pliku ELF, np. /usr/bin/CALC.ELF)
    const char* path = (const char*)arg;
    int i = 0;
    while (path && path[i] && i < 31) {
        t->name[i] = path[i];
        i++;
    }
    t->name[i] = '\0';
    
    // Setup initial registers
    t->regs.rip = (uint64_t)entry;
    t->regs.rsp = stack_top;
    t->regs.rbp = stack_top;
    t->regs.rflags = 0x202; // IF enabled
    t->regs.cs = 0x08; // Kernel Code
    t->regs.ss = 0x10; // Kernel Data
    t->regs.rdi = (uint64_t)arg; // Arg 1
    
    task_count++;
}

Registers* Scheduler::Schedule(Registers* regs) {
    if (task_count == 0) return regs;
    
    if (current_task != -1) {
        // Save current task registers
        tasks[current_task].regs = *regs;
    }
    
    // Round-robin
    do {
        current_task = (current_task + 1) % MAX_TASKS;
    } while (!tasks[current_task].active);
    
    return &tasks[current_task].regs;
}

void Scheduler::KillCurrentTask() {
    if (current_task != -1 && tasks[current_task].active) {
        tasks[current_task].active = false;
        task_count--;
    }
}

int Scheduler::GetTasks(TaskInfo* buffer, int max_count) {
    int count = 0;
    for (int idx = 0; idx < MAX_TASKS && count < max_count; idx++) {
        if (tasks[idx].active) {
            buffer[count].id = tasks[idx].id;
            for (int k = 0; k < 32; k++) {
                buffer[count].name[k] = tasks[idx].name[k];
            }
            count++;
        }
    }
    return count;
}

bool Scheduler::KillTaskById(uint64_t id) {
    if (id <= 2) return false; // Zabezpieczenie przed zabiciem kernela (PID 1 - idle, PID 2 - desktop)
    for (int idx = 0; idx < MAX_TASKS; idx++) {
        if (tasks[idx].active && tasks[idx].id == id) {
            tasks[idx].active = false;
            task_count--;
            return true;
        }
    }
    return false;
}

uint64_t Scheduler::GetCurrentTaskId() {
    if (current_task == -1) return 0;
    return tasks[current_task].id;
}
