#include <gui.h>
#include <stddef.h>

uint8_t canvas[32][32];
uint32_t current_color = 0; // index into paint_palette
const uint32_t paint_palette[8] = {
    0x000000, 0xFFFFFF, 0xFF0000, 0x00FF00, 
    0x0000FF, 0xFFFF00, 0x00FFFF, 0xFF00FF
};

void DrawAppButton(Window* win, int x, int y, int w, int h, const char* text) {
    gui_draw_rect(win, x, y, w, h, 0x808080);
    gui_draw_rect(win, x, y, w, 2, 0xFFFFFF);
    gui_draw_rect(win, x, y, 2, h, 0xFFFFFF);
    gui_draw_rect(win, x + w - 2, y, 2, h, 0x000000);
    gui_draw_rect(win, x, y + h - 2, w, 2, 0x000000);
    
    // Oblicz dlugosc tekstu
    int len = 0;
    while(text[len]) len++;
    
    int tx = x + (w - len * 8) / 2;
    int ty = y + (h - 16) / 2;
    gui_draw_string(win, text, tx, ty, 0x000000, 0x808080);
}

void PaintPaint(Window* win) {
    int win_x = 0;
    int win_y = 0;
    int width = win->width;
    int height = win->height;
    
    gui_draw_rect(win, win_x, win_y, width, height, 0x808080); // Tło

    int cell_size = 6;
    int grid_x = win_x + (width - 32 * cell_size) / 2;
    int grid_y = win_y + 8;
    
    // Obramowanie płótna
    gui_draw_rect(win, grid_x - 2, grid_y - 2, 32 * cell_size + 4, 32 * cell_size + 4, 0x000000);
    gui_draw_rect(win, grid_x - 1, grid_y - 1, 32 * cell_size + 2, 32 * cell_size + 2, 0xFFFFFF);
    
    // Rysowanie siatki
    for(int y=0; y<32; y++) {
        for(int x=0; x<32; x++) {
            gui_draw_rect(win, grid_x + x * cell_size, grid_y + y * cell_size, cell_size, cell_size, paint_palette[canvas[x][y]]);
        }
    }
    
    // Rysowanie panelu narzędzi (60 px wysokości)
    int palette_y = win_y + height - 60;
    gui_draw_rect(win, win_x + 4, palette_y, width - 8, 56, 0xC0C0C0);
    gui_draw_rect(win, win_x + 4, palette_y, width - 8, 2, 0xFFFFFF);
    gui_draw_rect(win, win_x + 4, palette_y, 2, 56, 0xFFFFFF);
    gui_draw_rect(win, win_x + width - 6, palette_y, 2, 56, 0x000000);
    gui_draw_rect(win, win_x + 4, palette_y + 54, width - 8, 2, 0x000000);
    
    // Paleta barw (pierwszy rząd)
    for(int i=0; i<8; i++) {
        int px = win_x + 12 + i * 28;
        int py = palette_y + 6;
        gui_draw_rect(win, px, py, 20, 20, paint_palette[i]);
        if (current_color == i) {
            gui_draw_rect(win, px-2, py-2, 24, 2, 0x000000);
            gui_draw_rect(win, px-2, py-2, 2, 24, 0x000000);
            gui_draw_rect(win, px+22, py-2, 2, 24, 0x000000);
            gui_draw_rect(win, px-2, py+22, 24, 2, 0x000000);
        }
    }
    
    // Rysowanie przycisków
    int btn_py = palette_y + 32;
    // Gumka (traktowana jako kolor 1, ale z UI ułatwieniem)
    DrawAppButton(win, win_x + 12, btn_py, 60, 20, "GUMKA");
    if (current_color == 1) { // Podświetl jeśli wybrana gumka (biały)
        gui_draw_rect(win, win_x + 10, btn_py - 2, 64, 2, 0x000000);
        gui_draw_rect(win, win_x + 10, btn_py - 2, 2, 24, 0x000000);
        gui_draw_rect(win, win_x + 72, btn_py - 2, 2, 24, 0x000000);
        gui_draw_rect(win, win_x + 10, btn_py + 20, 64, 2, 0x000000);
    }
    
    // CZYŚĆ
    DrawAppButton(win, win_x + 80, btn_py, 70, 20, "CZYSC");
    
    // ZAPISZ
    DrawAppButton(win, win_x + 160, btn_py, 70, 20, "ZAPISZ");
}

void DrawPixel(Window* win, int local_x, int local_y) {
    if (!win) return;
    int width = win->width;
    int cell_size = 6;
    int grid_x = (width - 32 * cell_size) / 2;
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

void OnMouseClick(Window* win, int local_x, int local_y) {
    if (!win) return;
    int height = win->height;
    int palette_y = height - 60;
    
    if (local_y >= palette_y && local_y <= palette_y + 56) {
        if (local_y <= palette_y + 28) {
            // Pierwszy rząd - Kolory
            for(int i=0; i<8; i++) {
                int px = 12 + i * 28;
                if (local_x >= px && local_x <= px + 20) {
                    current_color = i;
                    break;
                }
            }
        } else {
            // Drugi rząd - Przyciski (od y = palette_y + 32 do y = palette_y + 52)
            if (local_x >= 12 && local_x <= 72) {
                // GUMKA
                current_color = 1; // Biały
            } else if (local_x >= 80 && local_x <= 150) {
                // CZYŚĆ
                for(int y=0; y<32; y++) {
                    for(int x=0; x<32; x++) {
                        canvas[x][y] = 1;
                    }
                }
            } else if (local_x >= 160 && local_x <= 230) {
                // ZAPISZ - Na razie nie mamy syscalla do zapisu pliku z poziomu userspace!
                // Zostawmy na razie to jako "todo" lub wywolajmy po prostu rysowanie pustego info
            }
        }
    } else {
        DrawPixel(win, local_x, local_y);
    }
}

int main() {
    Window* win = sys_create_window(200, 50, 260, 300, "Paint");
    if (!win) return 1;
    
    for(int y=0; y<32; y++) {
        for(int x=0; x<32; x++) {
            canvas[x][y] = 1; // Biały
        }
    }
    
    PaintPaint(win);
    sys_update_window(win);
    
    Event ev;
    while (1) {
        if (sys_get_event(win, &ev)) {
            if (ev.type == EventWindowClose) {
                break;
            } else if (ev.type == EventMouseClick) {
                OnMouseClick(win, ev.mouse_x, ev.mouse_y);
                PaintPaint(win);
                sys_update_window(win);
            } else if (ev.type == EventMouseMove) {
                // Gdybyśmy wspierali MouseMove
            }
        }
    }
    
    sys_exit();
    return 0;
}
