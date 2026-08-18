#include "ps2_mouse.h"
#include "../serial.h"
#include "pic.h"

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) : "memory");
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) : "memory");
    return ret;
}

static inline void mouse_wait(uint8_t a_type) {
    uint32_t timeout = 100000;
    if (a_type == 0) {
        while (timeout--) {
            if ((inb(0x64) & 1) == 1) return;
        }
    } else {
        while (timeout--) {
            if ((inb(0x64) & 2) == 0) return;
        }
    }
}

static inline void mouse_write(uint8_t write) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, write);
}

static inline uint8_t mouse_read() {
    mouse_wait(0);
    return inb(0x60);
}

// VMMouse definitions
#define VMWARE_MAGIC 0x564D5868
#define VMWARE_PORT  0x5658

#define VMMOUSE_READ_ID 0x45414552
#define VMMOUSE_REQUEST_ABSOLUTE 0x53424152

#define VMMOUSE_DATA 39
#define VMMOUSE_STATUS 40
#define VMMOUSE_COMMAND 41

#define VMMOUSE_QEMU_VERSION 0x3442554a

struct VMWareCommand {
    uint32_t ax, bx, cx, dx, si, di;
};

static void vmmouse_send(VMWareCommand& cmd) {
    cmd.ax = VMWARE_MAGIC;
    cmd.dx = VMWARE_PORT;
    asm volatile("in %%dx, %%eax"
        : "=a"(cmd.ax), "=b"(cmd.bx), "=c"(cmd.cx), "=d"(cmd.dx), "=S"(cmd.si), "=D"(cmd.di)
        : "a"(cmd.ax), "b"(cmd.bx), "c"(cmd.cx), "d"(cmd.dx), "S"(cmd.si), "D"(cmd.di)
        : "memory");
}

static bool vmmouse_enabled = false;

int32_t mouse_x = 0;
int32_t mouse_y = 0;
bool mouse_left = false;
bool mouse_right = false;
bool mouse_middle = false;

static uint8_t mouse_cycle = 0;
static int8_t mouse_byte[3];

void Mouse::Init() {
    uint8_t status;
    
    // Enable auxiliary mouse device
    mouse_wait(1);
    outb(0x64, 0xA8);
    
    // Enable IRQ12
    mouse_wait(1);
    outb(0x64, 0x20);
    mouse_wait(0);
    status = (inb(0x60) | 2);
    mouse_wait(1);
    outb(0x64, 0x60);
    mouse_wait(1);
    outb(0x60, status);
    
    // Set default settings
    mouse_write(0xF6);
    mouse_read();
    
    // Enable data reporting
    mouse_write(0xF4);
    mouse_read();
    
    // Try to detect and enable VMMouse (Absolute mode)
    VMWareCommand cmd = {0,0,0,0,0,0};
    cmd.bx = VMMOUSE_READ_ID;
    cmd.cx = VMMOUSE_COMMAND;
    vmmouse_send(cmd);
    
    cmd.bx = 1;
    cmd.cx = VMMOUSE_DATA;
    vmmouse_send(cmd);
    
    if (cmd.ax == VMMOUSE_QEMU_VERSION) {
        // Request Absolute Mode
        cmd.bx = 0;
        cmd.cx = VMMOUSE_STATUS;
        vmmouse_send(cmd);
        
        cmd.bx = VMMOUSE_REQUEST_ABSOLUTE;
        cmd.cx = VMMOUSE_COMMAND;
        vmmouse_send(cmd);
        
        vmmouse_enabled = true;
        SerialPort::WriteString("PS/2 Mouse: VMMouse (Absolute) Enabled.\n");
    } else {
        SerialPort::WriteString("PS/2 Mouse: Standard Relative Mode.\n");
    }
    
    PIC::ClearMask(12); // Unmask IRQ12
}

void Mouse::HandleInterrupt() {
    uint8_t status = inb(0x64);
    if (!(status & 0x20)) return; // Check if it's from mouse
    
    uint8_t ps2_data = inb(0x60); // Must read to clear interrupt
    
    if (vmmouse_enabled) {
        int max_iters = 128;
        while (max_iters--) {
            VMWareCommand cmd = {0,0,0,0,0,0};
            cmd.bx = 0;
            cmd.cx = VMMOUSE_STATUS;
            vmmouse_send(cmd);
            
            uint16_t size = cmd.ax & 0xFFFF;
            if (size == 0xFFFF || size == 0) break;
            
            cmd.bx = 4;
            cmd.cx = VMMOUSE_DATA;
            vmmouse_send(cmd);
            
            uint32_t buttons = cmd.ax & 0xFFFF;
            // Absolutne X i Y (z QEMU: 0 do 0xFFFF).
            // Zakładając rozdzielczość 800x600.
            mouse_x = (cmd.bx * 800) / 0xFFFF;
            mouse_y = (cmd.cx * 600) / 0xFFFF;
            
            mouse_left = buttons & 0x20;
            mouse_right = buttons & 0x10;
            mouse_middle = buttons & 0x08;
        }
        return;
    }
    
    // Relative PS/2 mode fallback
    switch (mouse_cycle) {
        case 0:
            mouse_byte[0] = ps2_data;
            if (mouse_byte[0] & 0x08) {
                mouse_cycle++;
            }
            break;
        case 1:
            mouse_byte[1] = ps2_data;
            mouse_cycle++;
            break;
        case 2:
            mouse_byte[2] = ps2_data;
            
            int dx = mouse_byte[1] - ((mouse_byte[0] << 4) & 0x100);
            int dy = mouse_byte[2] - ((mouse_byte[0] << 3) & 0x100);
            
            mouse_x += dx;
            mouse_y -= dy;
            
            mouse_left = mouse_byte[0] & 0x01;
            mouse_right = mouse_byte[0] & 0x02;
            mouse_middle = mouse_byte[0] & 0x04;
            
            mouse_cycle = 0;
            break;
    }
}
