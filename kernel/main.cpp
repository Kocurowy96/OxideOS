#include <stdint.h>
#include <stddef.h>
#include "limine.h"
#include "serial.h"
#include "cpu/gdt.h"
#include "cpu/idt.h"
#include "drivers/pic.h"
#include "drivers/rtc.h"
#include "drivers/pit.h"
#include "drivers/pci.h"
#include "drivers/ac97.h"
#include "mem/pmm.h"
#include "mem/vmm.h"
#include "proc/sched.h"

// Set the base revision to 3, this is recommended.
LIMINE_BASE_REVISION(3)

__attribute__((used, section(".requests")))
volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

// Halt and catch fire function.
static void hcf(void) {
    asm("cli");
    for (;;) {
        asm("hlt");
    }
}

static void print_uint64(uint64_t val) {
    char buf[20] = {0};
    int i = 0;
    if (val == 0) buf[i++] = '0';
    while (val > 0) { buf[i++] = '0' + (val % 10); val /= 10; }
    for (int j = i - 1; j >= 0; j--) SerialPort::WriteChar(buf[j]);
}

#include "gui/compositor.h"
#include "gui/osod.h"
#include "gui/apps.h"

extern "C" void* memset(void* dest, int val, uint64_t len) {
    uint8_t* ptr = (uint8_t*)dest;
    while (len-- > 0)
        *ptr++ = (uint8_t)val;
    return dest;
}

extern "C" {
    void* __dso_handle = nullptr;
    int __cxa_atexit(void (*)(void *), void *, void *) { return 0; }
}

extern "C" void* memcpy(void* dest, const void* src, uint64_t len) {
    uint64_t* d64 = (uint64_t*)dest;
    const uint64_t* s64 = (const uint64_t*)src;
    while (len >= 8) {
        *d64++ = *s64++;
        len -= 8;
    }
    uint8_t* d8 = (uint8_t*)d64;
    const uint8_t* s8 = (const uint8_t*)s64;
    while (len > 0) {
        *d8++ = *s8++;
        len--;
    }
    return dest;
}

void operator delete(void* p, unsigned long) {}
void operator delete[](void* p, unsigned long) {}
void operator delete(void* p) {}
void operator delete[](void* p) {}

#include "gui/fb.h"
#include "drivers/ps2_kbd.h"
#include "drivers/ps2_mouse.h"
#include "drivers/ata.h"
#include "fs/vfs.h"
#include "fs/elf.h"

void DesktopTask(void* arg) {
    Compositor::Init();
    
    // Zostawiamy puste, nowa aplikacja powitalna to HELLO.ELF z userspace'u
    
    uint8_t* wav_buffer = nullptr;
    uint32_t wav_size = 0;
    // Odtwarzamy dźwięk startowy (jeśli wbudowany/dostarczony przez użytkownika)
    if (VFS::ReadFile("/STARTUP.WAV", &wav_buffer, &wav_size)) {
        AC97::PlayWAV(wav_buffer);
    }
    
    while (1) {
        Compositor::Render();
        // Krótkie opóźnienie aby nie zablokować całkowicie zasobów
        for (volatile int i = 0; i < 50000; i++);
    }
}

void ExecAppTask(void* path_ptr) {
    const char* path = (const char*)path_ptr;
    uint8_t* buffer = nullptr;
    uint32_t size = 0;
    
    if (VFS::ReadFile(path, &buffer, &size)) {
        SerialPort::WriteString("ExecAppTask: Loaded ");
        SerialPort::WriteString(path);
        SerialPort::WriteString(". Jumping to Ring 3...\n");
        uint64_t entry_point = ELF::Load(buffer);
        
        if (entry_point) {
            // Allocate a user stack
            void* user_stack = PMM::AllocatePage();
            uint64_t user_stack_top = ((uint64_t)user_stack) + 4096;
            
            // Map the stack in user space (e.g., at a fixed high address)
            // But if we run multiple tasks, we need separate stacks!
            // Wait, we don't have separate page tables per process yet.
            // So we need unique virtual addresses for each process stack!
            // Let's use 0x700000000000 + (TaskID * 0x10000) for stack.
            // Wait, we don't know TaskID here easily without a syscall.
            // Let's generate a quick unique stack vaddr based on a counter.
            static uint64_t next_stack = 0x700000000000;
            uint64_t stack_vaddr = next_stack - 4096;
            next_stack -= 0x10000;
            
            VMM::MapPage((uint64_t)user_stack, stack_vaddr, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
            
            asm volatile(
                "mov $0x23, %%ax\n"
                "mov %%ax, %%ds\n"
                "mov %%ax, %%es\n"
                "mov %%ax, %%fs\n"
                "mov %%ax, %%gs\n"
                "push $0x23\n" // SS
                "push %1\n"    // RSP
                "push $0x202\n" // RFLAGS
                "push $0x1B\n" // CS
                "push %0\n"    // RIP
                "iretq\n"
                : : "r"(entry_point), "r"(stack_vaddr + 4096) : "ax", "memory"
            );
        }
    } else {
        SerialPort::WriteString("ExecAppTask: Failed to load ");
        SerialPort::WriteString(path);
        SerialPort::WriteString("\n");
    }
    
    Scheduler::KillCurrentTask();
    while (1) {
        asm volatile("hlt");
    }
}

extern "C" void _start(void) {
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        hcf();
    }

    SerialPort::Init();
    SerialPort::WriteString("OxideOS: Booting up...\n");

    // Phase 2A Initialization
    GDT::Init();
    IDT::Init();
    PIC::Init();
    RTC::PrintTime();

    // Phase 2B Initialization
    SerialPort::WriteString("OxideOS: Initializing Memory...\n");
    PMM::Init();
    
    SerialPort::WriteString("  Total RAM: ");
    print_uint64(PMM::GetTotalMemory() / 1024 / 1024);
    SerialPort::WriteString(" MB\n");
    
    SerialPort::WriteString("  Free RAM:  ");
    print_uint64(PMM::GetFreeMemory() / 1024 / 1024);
    SerialPort::WriteString(" MB\n");

    VMM::Init();
    SerialPort::WriteString("OxideOS: Paging initialized.\n");

    extern volatile struct limine_hhdm_request hhdm_request;
    // Initialize TSS stack for interrupts from User Mode
    void* tss_stack = PMM::AllocatePage();
    uint64_t tss_stack_vaddr = (uint64_t)tss_stack + hhdm_request.response->offset;
    GDT::SetTSSStack((void*)(tss_stack_vaddr + 4096));

    // Test Allocation
    void* test_page = PMM::AllocatePage();
    if (test_page) {
        SerialPort::WriteString("OxideOS: Page allocation test successful!\n");
        PMM::FreePage(test_page);
    } else {
        SerialPort::WriteString("OxideOS: Page allocation failed!\n");
    }

    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        SerialPort::WriteString("OxideOS: No framebuffer found. Halting.\n");
        hcf();
    }
    Framebuffer::Init();
    SerialPort::WriteString("OxideOS: Framebuffer acquired.\n");
    
    // Phase 4 & 5
    Keyboard::Init();
    Mouse::Init();
    ATA::Init();
    VFS::Init();
    PCI::Init();
    AC97::Init();
    
    // Phase 3 Initialization
    Scheduler::Init();
    
    // Start the User App (HELLO.ELF) via Scheduler
    Scheduler::CreateTask((void (*)(void*))ExecAppTask, (void*)"/usr/bin/HELLO.ELF");
    
    // Start desktop rendering task
    Scheduler::CreateTask((void (*)(void*))DesktopTask, nullptr);
    
    PIT::Init(100); // 100 Hz = 10ms tick

    SerialPort::WriteString("OxideOS: GUI and Multitasking initialized. Idling main thread...\n");
    
    asm volatile("sti");
    
    // Idle loop
    while (1) {
        asm volatile("hlt");
    }
}
