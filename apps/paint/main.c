#include <gui.h>
#include <stddef.h>

uint8_t canvas[32][32];
uint32_t current_color = 0;
const uint32_t paint_palette[8] = {
    0x000000, 0xFFFFFF, 0xFF0000, 0x00FF00, 
    0x0000FF, 0xFFFF00, 0x00FFFF, 0xFF00FF
};

int win_id;
uint32_t* fb;
int win_w = 260;
int win_h = 300;

void DrawAppButton(int x, int y, int w, int h, const char* text) {
    gui_draw_rect(fb, win_w, x, y, w, h, 0x808080);
    gui_draw_rect(fb, win_w, x, y, w, 2, 0xFFFFFF);
    gui_draw_rect(fb, win_w, x, y, 2, h, 0xFFFFFF);
    gui_draw_rect(fb, win_w, x + w - 2, y, 2, h, 0x000000);
    gui_draw_rect(fb, win_w, x, y + h - 2, w, 2, 0x000000);
    
    int len = 0;
    while(text[len]) len++;
    
    int tx = x + (w - len * 8) / 2;
    int ty = y + (h - 8) / 2;
    gui_draw_string(fb, win_w, text, tx, ty, 0x000000, 0x808080);
}

void PaintPaint() {
    gui_draw_rect(fb, win_w, 0, 0, win_w, win_h, 0x808080);

    int cell_size = 6;
    int grid_x = (win_w - 32 * cell_size) / 2;
    int grid_y = 8;
    
    gui_draw_rect(fb, win_w, grid_x - 2, grid_y - 2, 32 * cell_size + 4, 32 * cell_size + 4, 0x000000);
    gui_draw_rect(fb, win_w, grid_x - 1, grid_y - 1, 32 * cell_size + 2, 32 * cell_size + 2, 0xFFFFFF);
    
    for(int y=0; y<32; y++) {
        for(int x=0; x<32; x++) {
            gui_draw_rect(fb, win_w, grid_x + x * cell_size, grid_y + y * cell_size, cell_size, cell_size, paint_palette[canvas[x][y]]);
        }
    }
    
    int palette_y = win_h - 60;
    gui_draw_rect(fb, win_w, 4, palette_y, win_w - 8, 56, 0xC0C0C0);
    gui_draw_rect(fb, win_w, 4, palette_y, win_w - 8, 2, 0xFFFFFF);
    gui_draw_rect(fb, win_w, 4, palette_y, 2, 56, 0xFFFFFF);
    gui_draw_rect(fb, win_w, win_w - 6, palette_y, 2, 56, 0x000000);
    gui_draw_rect(fb, win_w, 4, palette_y + 54, win_w - 8, 2, 0x000000);
    
    for(int i=0; i<8; i++) {
        int px = 12 + i * 28;
        int py = palette_y + 6;
        gui_draw_rect(fb, win_w, px, py, 20, 20, paint_palette[i]);
        if (current_color == (uint32_t)i) {
            gui_draw_rect(fb, win_w, px-2, py-2, 24, 2, 0x000000);
            gui_draw_rect(fb, win_w, px-2, py-2, 2, 24, 0x000000);
            gui_draw_rect(fb, win_w, px+22, py-2, 2, 24, 0x000000);
            gui_draw_rect(fb, win_w, px-2, py+22, 24, 2, 0x000000);
        }
    }
    
    int btn_py = palette_y + 32;
    DrawAppButton(12, btn_py, 60, 20, "GUMKA");
    if (current_color == 1) { 
        gui_draw_rect(fb, win_w, 10, btn_py - 2, 64, 2, 0x000000);
        gui_draw_rect(fb, win_w, 10, btn_py - 2, 2, 24, 0x000000);
        gui_draw_rect(fb, win_w, 72, btn_py - 2, 2, 24, 0x000000);
        gui_draw_rect(fb, win_w, 10, btn_py + 20, 64, 2, 0x000000);
    }
    
    DrawAppButton(80, btn_py, 70, 20, "CZYSC");
    DrawAppButton(160, btn_py, 70, 20, "ZAPISZ");
}

void DrawPixel(int local_x, int local_y) {
    int cell_size = 6;
    int grid_x = (win_w - 32 * cell_size) / 2;
    int grid_y = 8;
    
    if (local_x >= grid_x && local_x < grid_x + 32 * cell_size &&
        local_y >= grid_y && local_y < grid_y + 32 * cell_size) {
        int cx = (local_x - grid_x) / cell_size;
        int cy = (local_y - grid_y) / cell_size;
        if (cx >= 0 && cx < 32 && cy >= 0 && cy < 32) {
            canvas[cx][cy] = current_color;
        }
    }
}

void OnMouseClick(int local_x, int local_y) {
    int palette_y = win_h - 60;
    
    if (local_y >= palette_y && local_y <= palette_y + 56) {
        if (local_y <= palette_y + 28) {
            for(int i=0; i<8; i++) {
                int px = 12 + i * 28;
                if (local_x >= px && local_x <= px + 20) {
                    current_color = i;
                    break;
                }
            }
        } else {
            if (local_x >= 12 && local_x <= 72) {
                current_color = 1; 
            } else if (local_x >= 80 && local_x <= 150) {
                for(int y=0; y<32; y++) {
                    for(int x=0; x<32; x++) {
                        canvas[x][y] = 1;
                    }
                }
            } else if (local_x >= 160 && local_x <= 230) {
                sys_write_file("/DOCS/PAINT.RAW", (const uint8_t*)canvas, sizeof(canvas));
            }
        }
    } else {
        DrawPixel(local_x, local_y);
    }
}

void _start() {
    win_id = sys_create_window("Paint", win_w, win_h, 200, 50, &fb);
    if (win_id < 0 || !fb) sys_exit();
    
    for(int y=0; y<32; y++) {
        for(int x=0; x<32; x++) {
            canvas[x][y] = 1; 
        }
    }
    
    PaintPaint();
    sys_update_window(win_id);
    
    struct WindowEvent ev;
    while (1) {
        if (sys_get_event(win_id, &ev)) {
            if (ev.type == 1) { 
                OnMouseClick(ev.x, ev.y);
                PaintPaint();
                sys_update_window(win_id);
            }
        }
        for (volatile int i = 0; i < 10000; i++);
    }
}
