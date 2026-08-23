#pragma once
#include <stdint.h>

#pragma pack(push, 1)
struct BMPHeader {
    uint16_t signature;
    uint32_t file_size;
    uint32_t reserved;
    uint32_t data_offset;
};

struct BMPInfoHeader {
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bit_count;
    uint32_t compression;
    uint32_t image_size;
    int32_t x_pixels_per_m;
    int32_t y_pixels_per_m;
    uint32_t colors_used;
    uint32_t colors_important;
};
#pragma pack(pop)

class BMP {
public:
    static void Draw(void* bmp_data, int32_t x, int32_t y);
    static void DrawToBuffer(void* bmp_data, uint32_t* buffer, int buf_w, int buf_h, int32_t x, int32_t y);
};
