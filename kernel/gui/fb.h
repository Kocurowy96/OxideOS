#pragma once
#include <stdint.h>

class Framebuffer {
public:
    static void Init();
    static void PutPixel(uint32_t x, uint32_t y, uint32_t color);
    static void DrawRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
    static void Clear(uint32_t color);
    static void DrawChar(char c, uint32_t x, uint32_t y, uint32_t fg_color, uint32_t bg_color);
    static void DrawString(const char* str, uint32_t x, uint32_t y, uint32_t fg_color, uint32_t bg_color);
    
    static uint32_t GetWidth();
    static uint32_t GetHeight();
    static void SwapBuffers();
};
