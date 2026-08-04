#include <stdint.h>
#include <stddef.h>
#include "limine.h"
#include "serial.h"
#include "cpu/gdt.h"
#include "cpu/idt.h"
#include "drivers/pic.h"
#include "drivers/rtc.h"
#include "mem/pmm.h"
#include "mem/vmm.h"

// Set the base revision to 3, this is recommended.
LIMINE_BASE_REVISION(3)

__attribute__((used, section(".requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
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

    SerialPort::WriteString("OxideOS: Framebuffer acquired.\n");
    SerialPort::WriteString("OxideOS: All systems nominal. Idling...\n");
    
    // Idle loop
    while (1) {
        asm("hlt");
    }
}
