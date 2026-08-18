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

extern "C" void* memset(void* dest, int val, uint64_t len) {
    uint8_t* ptr = (uint8_t*)dest;
    while (len-- > 0)
        *ptr++ = (uint8_t)val;
    return dest;
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

#include "gui/fb.h"
#include "drivers/ps2_kbd.h"
#include "drivers/ps2_mouse.h"
#include "drivers/ata.h"
#include "fs/vfs.h"
#include "fs/elf.h"

void DesktopTask() {
    Compositor::Init();
    
    static Window test_win(150, 150, 400, 250, "Witaj w OxideOS!");
    Compositor::AddWindow(&test_win);
    
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

void UserAppTask() {
    uint8_t* buffer = nullptr;
    uint32_t size = 0;
    
    if (VFS::ReadFile("/HELLO.ELF", &buffer, &size)) {
        SerialPort::WriteString("UserAppTask: Loaded HELLO.ELF. Jumping to Ring 3...\n");
        uint64_t entry_point = ELF::Load(buffer);
        if (entry_point != 0) {
            // Allocate a user stack
            void* user_stack = PMM::AllocatePage();
            uint64_t user_stack_top = ((uint64_t)user_stack) + 4096;
            
            // Map the stack in user space (e.g., at a fixed high address)
            uint64_t stack_vaddr = 0x700000000000 - 4096; // Canonical user address
            VMM::MapPage((uint64_t)user_stack, stack_vaddr, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
            
            SwitchToUserMode(entry_point, stack_vaddr + 4096);
        } else {
            SerialPort::WriteString("UserAppTask: Failed to load ELF sections.\n");
        }
    } else {
        SerialPort::WriteString("UserAppTask: Failed to read hello.elf from VFS.\n");
    }
    
    // Fallback if failed
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
    Scheduler::CreateTask(DesktopTask);
    Scheduler::CreateTask(UserAppTask);
    
    PIT::Init(100); // 100 Hz = 10ms tick

    SerialPort::WriteString("OxideOS: GUI and Multitasking initialized. Idling main thread...\n");
    
    asm volatile("sti");
    
    // Idle loop
    while (1) {
        asm volatile("hlt");
    }
}
