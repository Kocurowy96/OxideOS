#pragma once
#include <stdint.h>

class Framebuffer {
public:
    static void Init();
    static void PutPixel(int32_t x, int32_t y, uint32_t color);
    static void DrawRect(int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t color);
    static void Clear(uint32_t color);
    static void DrawChar(char c, int32_t x, int32_t y, uint32_t fg_color, uint32_t bg_color);
    static void DrawString(const char* str, int32_t x, int32_t y, uint32_t fg_color, uint32_t bg_color);
    
    static uint32_t GetWidth();
    static uint32_t GetHeight();
    static void SwapBuffers();
};
