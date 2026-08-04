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

void Scheduler::CreateTask(void (*entry)()) {
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
    
    // Allocate stack (1 page = 4096 bytes)
    void* stack = PMM::AllocatePage();
    uint64_t stack_top = (uint64_t)stack + 4096;
    if (hhdm_request.response != nullptr) {
        stack_top += hhdm_request.response->offset;
    }
    
    // Zero out registers
    for(size_t i = 0; i < sizeof(Registers); i++) {
        ((uint8_t*)&t->regs)[i] = 0;
    }
    
    // Setup initial registers
    t->regs.rip = (uint64_t)entry;
    t->regs.rsp = stack_top;
    t->regs.rbp = stack_top;
    t->regs.rflags = 0x202; // IF enabled
    t->regs.cs = 0x08; // Kernel Code
    t->regs.ss = 0x10; // Kernel Data
    
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
