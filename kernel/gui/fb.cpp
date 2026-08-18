#include "fb.h"
#include "../limine.h"
#include "../fonts/font8x8.h"

extern "C" void* memcpy(void* dest, const void* src, uint64_t len);

extern volatile struct limine_framebuffer_request framebuffer_request;

#include "../mem/pmm.h"
extern volatile struct limine_hhdm_request hhdm_request;

static struct limine_framebuffer* fb = nullptr;
static uint32_t* backbuffer = nullptr;

void Framebuffer::Init() {
    if (framebuffer_request.response == nullptr || framebuffer_request.response->framebuffer_count < 1) {
        return;
    }
    fb = framebuffer_request.response->framebuffers[0];
    
    size_t size = fb->width * fb->height * 4;
    size_t pages = (size + 4095) / 4096;
    void* phys = PMM::AllocatePages(pages);
    if (phys && hhdm_request.response) {
        backbuffer = (uint32_t*)((uint64_t)phys + hhdm_request.response->offset);
    }
}

uint32_t Framebuffer::GetWidth() { return fb ? fb->width : 0; }
uint32_t Framebuffer::GetHeight() { return fb ? fb->height : 0; }

void Framebuffer::PutPixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!fb || x >= fb->width || y >= fb->height) return;
    if (backbuffer) {
        backbuffer[y * fb->width + x] = color;
    } else {
        uint32_t* screen = (uint32_t*)fb->address;
        screen[y * (fb->pitch / 4) + x] = color;
    }
}

void Framebuffer::SwapBuffers() {
    if (!fb || !backbuffer) return;
    uint32_t* screen = (uint32_t*)fb->address;
    
    if (fb->pitch == fb->width * 4) {
        memcpy(screen, backbuffer, fb->width * fb->height * 4);
    } else {
        for (uint32_t y = 0; y < fb->height; y++) {
            for (uint32_t x = 0; x < fb->width; x++) {
                screen[y * (fb->pitch / 4) + x] = backbuffer[y * fb->width + x];
            }
        }
    }
}

void Framebuffer::DrawRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!fb) return;
    if (x >= fb->width || y >= fb->height) return;
    if (x + w > fb->width) w = fb->width - x;
    if (y + h > fb->height) h = fb->height - y;
    
    if (backbuffer) {
        for (uint32_t i = 0; i < h; i++) {
            uint32_t* row = &backbuffer[(y + i) * fb->width + x];
            for (uint32_t j = 0; j < w; j++) {
                row[j] = color;
            }
        }
    } else {
        for (uint32_t i = 0; i < h; i++) {
            for (uint32_t j = 0; j < w; j++) {
                PutPixel(x + j, y + i, color);
            }
        }
    }
}

void Framebuffer::Clear(uint32_t color) {
    if (!fb) return;
    if (backbuffer) {
        uint64_t color64 = ((uint64_t)color << 32) | color;
        uint64_t* ptr = (uint64_t*)backbuffer;
        size_t qwords = (fb->width * fb->height) / 2;
        while(qwords--) {
            *ptr++ = color64;
        }
    } else {
        DrawRect(0, 0, fb->width, fb->height, color);
    }
}

void Framebuffer::DrawChar(char c, uint32_t x, uint32_t y, uint32_t fg_color, uint32_t bg_color) {
    if ((uint8_t)c > 127) return; // Only ASCII
    for (int row = 0; row < 8; row++) {
        uint8_t row_data = font8x8_basic[(uint8_t)c][row];
        for (int col = 0; col < 8; col++) {
            if (row_data & (1 << col)) {
                PutPixel(x + col, y + row, fg_color);
            } else {
                PutPixel(x + col, y + row, bg_color);
            }
        }
    }
}

void Framebuffer::DrawString(const char* str, uint32_t x, uint32_t y, uint32_t fg_color, uint32_t bg_color) {
    uint32_t current_x = x;
    uint32_t current_y = y;
    while (*str) {
        if (*str == '\n') {
            current_x = x;
            current_y += 8;
        } else {
            DrawChar(*str, current_x, current_y, fg_color, bg_color);
            current_x += 8;
        }
        str++;
    }
}
