#include "bmp.h"
#include "fb.h"
#include "../serial.h"

void BMP::Draw(void* bmp_data, int32_t x, int32_t y) {
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
            uint8_t a = (bytes_per_pixel == 4) ? row_data[col * bytes_per_pixel + 3] : 255;
            
            // Fallback for 24-bit: magenta is transparent
            if (bytes_per_pixel == 3 && r == 255 && g == 0 && b == 255) a = 0;
            
            if (a == 0) continue;
            
            int draw_x = x + col;
            int draw_y = y + row;
            
            if (a == 255) {
                Framebuffer::PutPixel(draw_x, draw_y, (r << 16) | (g << 8) | b);
            } else {
                uint32_t bg = Framebuffer::GetPixel(draw_x, draw_y);
                uint8_t bg_r = (bg >> 16) & 0xFF;
                uint8_t bg_g = (bg >> 8) & 0xFF;
                uint8_t bg_b = bg & 0xFF;
                
                uint8_t final_r = (r * a + bg_r * (255 - a)) / 255;
                uint8_t final_g = (g * a + bg_g * (255 - a)) / 255;
                uint8_t final_b = (b * a + bg_b * (255 - a)) / 255;
                
                Framebuffer::PutPixel(draw_x, draw_y, (final_r << 16) | (final_g << 8) | final_b);
            }
        }
    }
}

void BMP::DrawToBuffer(void* bmp_data, uint32_t* buffer, int buf_w, int buf_h, int32_t x, int32_t y) {
    if (!bmp_data || !buffer) return;
    
    BMPHeader* header = (BMPHeader*)bmp_data;
    if (header->signature != 0x4D42) return;
    
    BMPInfoHeader* info = (BMPInfoHeader*)((uint8_t*)bmp_data + sizeof(BMPHeader));
    if (info->bit_count != 24 && info->bit_count != 32) return;
    if (info->compression != 0) return;
    
    uint8_t* pixel_data = (uint8_t*)bmp_data + header->data_offset;
    int width = info->width;
    int height = info->height;
    int bytes_per_pixel = info->bit_count / 8;
    int row_stride = (width * bytes_per_pixel + 3) & ~3;
    
    for (int row = 0; row < height; row++) {
        int inverted_row = height - 1 - row;
        uint8_t* row_data = pixel_data + (inverted_row * row_stride);
        
        for (int col = 0; col < width; col++) {
            uint8_t b = row_data[col * bytes_per_pixel];
            uint8_t g = row_data[col * bytes_per_pixel + 1];
            uint8_t r = row_data[col * bytes_per_pixel + 2];
            uint8_t a = (bytes_per_pixel == 4) ? row_data[col * bytes_per_pixel + 3] : 255;
            
            if (bytes_per_pixel == 3 && r == 255 && g == 0 && b == 255) a = 0;
            
            if (a == 0) continue;
            
            int draw_x = x + col;
            int draw_y = y + row;
            if (draw_x >= 0 && draw_x < buf_w && draw_y >= 0 && draw_y < buf_h) {
                if (a == 255) {
                    buffer[draw_y * buf_w + draw_x] = (r << 16) | (g << 8) | b;
                } else {
                    uint32_t bg = buffer[draw_y * buf_w + draw_x];
                    uint8_t bg_r = (bg >> 16) & 0xFF;
                    uint8_t bg_g = (bg >> 8) & 0xFF;
                    uint8_t bg_b = bg & 0xFF;
                    
                    uint8_t final_r = (r * a + bg_r * (255 - a)) / 255;
                    uint8_t final_g = (g * a + bg_g * (255 - a)) / 255;
                    uint8_t final_b = (b * a + bg_b * (255 - a)) / 255;
                    
                    buffer[draw_y * buf_w + draw_x] = (final_r << 16) | (final_g << 8) | final_b;
                }
            }
        }
    }
}
