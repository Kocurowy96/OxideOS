#include "fb.h"
#include "../limine.h"
#include "../fonts/font8x8.h"

extern volatile struct limine_framebuffer_request framebuffer_request;

static struct limine_framebuffer* fb = nullptr;

void Framebuffer::Init() {
    if (framebuffer_request.response == nullptr || framebuffer_request.response->framebuffer_count < 1) {
        return;
    }
    fb = framebuffer_request.response->framebuffers[0];
}

uint32_t Framebuffer::GetWidth() { return fb ? fb->width : 0; }
uint32_t Framebuffer::GetHeight() { return fb ? fb->height : 0; }

void Framebuffer::PutPixel(uint32_t x, uint32_t y, uint32_t color) {
    if (!fb || x >= fb->width || y >= fb->height) return;
    uint32_t* screen = (uint32_t*)fb->address;
    screen[y * (fb->pitch / 4) + x] = color;
}

void Framebuffer::DrawRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!fb) return;
    for (uint32_t i = 0; i < h; i++) {
        for (uint32_t j = 0; j < w; j++) {
            PutPixel(x + j, y + i, color);
        }
    }
}

void Framebuffer::Clear(uint32_t color) {
    if (!fb) return;
    DrawRect(0, 0, fb->width, fb->height, color);
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
