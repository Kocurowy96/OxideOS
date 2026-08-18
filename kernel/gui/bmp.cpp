#include "bmp.h"
#include "fb.h"
#include "../serial.h"

void BMP::Draw(void* bmp_data, uint32_t x, uint32_t y) {
    if (!bmp_data) return;
    
    BMPHeader* header = (BMPHeader*)bmp_data;
    if (header->signature != 0x4D42) { // 'BM'
        SerialPort::WriteString("BMP: Invalid signature!\n");
        return;
    }
    
    BMPInfoHeader* info = (BMPInfoHeader*)((uint8_t*)bmp_data + sizeof(BMPHeader));
    
    if (info->bit_count != 24 && info->bit_count != 32) {
        SerialPort::WriteString("BMP: Only 24-bit or 32-bit supported!\n");
        return;
    }
    
    if (info->compression != 0) {
        SerialPort::WriteString("BMP: Compression not supported!\n");
        return;
    }
    
    uint8_t* pixel_data = (uint8_t*)bmp_data + header->data_offset;
    
    int width = info->width;
    int height = info->height;
    int bytes_per_pixel = info->bit_count / 8;
    
    int row_stride = (width * bytes_per_pixel + 3) & ~3; // 4-byte aligned
    
    for (int row = 0; row < height; row++) {
        // BMP is stored bottom-up
        int inverted_row = height - 1 - row;
        uint8_t* row_data = pixel_data + (inverted_row * row_stride);
        
        for (int col = 0; col < width; col++) {
            uint8_t b = row_data[col * bytes_per_pixel];
            uint8_t g = row_data[col * bytes_per_pixel + 1];
            uint8_t r = row_data[col * bytes_per_pixel + 2];
            // alpha is ignored for 24-bit, and for 32-bit it's byte 3 (not always reliable)
            uint32_t color = (r << 16) | (g << 8) | b;
            
            Framebuffer::PutPixel(x + col, y + row, color);
        }
    }
}
