#include <stdint.h>
#include <stddef.h>
#include "limine.h"
#include "serial.h"
#include "cpu/gdt.h"
#include "cpu/idt.h"
#include "drivers/pic.h"
#include "drivers/rtc.h"

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

extern "C" void _start(void) {
    if (LIMINE_BASE_REVISION_SUPPORTED == false) {
        hcf();
    }

    SerialPort::Init();
    SerialPort::WriteString("OxideOS: Booting up...\n");

    // Phase 2A Initialization
    GDT::Init();
    SerialPort::WriteString("OxideOS: GDT initialized.\n");
    
    IDT::Init();
    SerialPort::WriteString("OxideOS: IDT initialized.\n");
    
    PIC::Init();
    SerialPort::WriteString("OxideOS: PIC initialized.\n");
    
    RTC::PrintTime();

    if (framebuffer_request.response == NULL
     || framebuffer_request.response->framebuffer_count < 1) {
        SerialPort::WriteString("OxideOS: No framebuffer found. Halting.\n");
        hcf();
    }

    SerialPort::WriteString("OxideOS: Framebuffer acquired.\n");
    SerialPort::WriteString("Triggering an exception to test OSOD...\n");
    
    // Trigger Breakpoint Exception
    asm volatile("int $0x03");

    hcf();
}
