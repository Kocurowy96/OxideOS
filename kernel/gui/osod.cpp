#include "osod.h"
#include "fb.h"
#include "../serial.h"

static void uint64_to_hex(uint64_t val, char* buf) {
    const char* hex = "0123456789ABCDEF";
    for (int i = 15; i >= 0; i--) {
        buf[i] = hex[val & 0x0F];
        val >>= 4;
    }
    buf[16] = '\0';
}

void OSOD::Draw(Registers* regs) {
    Framebuffer::Init();
    
    // Background: Dark Gray/Rust
    uint32_t bg_color = 0x4A4A4A; // Szary
    uint32_t text_color = 0xFFFFFF; // Biały
    uint32_t title_color = 0xFF4444; // Czerwony dla błędu
    
    Framebuffer::Clear(bg_color);
    
    uint32_t x = 20;
    uint32_t y = 20;
    
    Framebuffer::DrawString("OxideOS: Kernel Panic! (OSOD)", x, y, title_color, bg_color);
    y += 24;
    
    Framebuffer::DrawString("A critical error has occurred and the system has been halted.", x, y, text_color, bg_color);
    y += 24;
    
    char buf[32];
    
    Framebuffer::DrawString("Exception: 0x", x, y, text_color, bg_color);
    uint64_to_hex(regs->int_no, buf);
    Framebuffer::DrawString(buf, x + 8*13, y, text_color, bg_color);
    y += 16;
    
    Framebuffer::DrawString("Error Code: 0x", x, y, text_color, bg_color);
    uint64_to_hex(regs->err_code, buf);
    Framebuffer::DrawString(buf, x + 8*14, y, text_color, bg_color);
    y += 32;

    Framebuffer::DrawString("RIP: 0x", x, y, text_color, bg_color);
    uint64_to_hex(regs->rip, buf);
    Framebuffer::DrawString(buf, x + 8*7, y, text_color, bg_color);
    
    Framebuffer::DrawString("RSP: 0x", x + 300, y, text_color, bg_color);
    uint64_to_hex(regs->rsp, buf);
    Framebuffer::DrawString(buf, x + 300 + 8*7, y, text_color, bg_color);
    y += 16;

    Framebuffer::DrawString("RAX: 0x", x, y, text_color, bg_color);
    uint64_to_hex(regs->rax, buf);
    Framebuffer::DrawString(buf, x + 8*7, y, text_color, bg_color);
    
    Framebuffer::DrawString("RBX: 0x", x + 300, y, text_color, bg_color);
    uint64_to_hex(regs->rbx, buf);
    Framebuffer::DrawString(buf, x + 300 + 8*7, y, text_color, bg_color);
    y += 16;
    
    Framebuffer::DrawString("Please restart the computer.", x, y + 40, title_color, bg_color);
}
