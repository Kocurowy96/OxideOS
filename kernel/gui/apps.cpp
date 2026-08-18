#include "apps.h"
#include "window.h"
#include "fb.h"
#include "../drivers/ac97.h"
#include "../fs/vfs.h"

// Helper function from compositor to draw buttons
void DrawAppButton(int x, int y, int w, int h, const char* text, bool pressed) {
    Framebuffer::DrawRect(x, y, w, h, 0xC0C0C0);
    if (pressed) {
        Framebuffer::DrawRect(x, y, w, 1, 0x000000); // inner shadow top
        Framebuffer::DrawRect(x, y, 1, h, 0x000000); // inner shadow left
    } else {
        Framebuffer::DrawRect(x, y, w, 1, 0xFFFFFF); // highlight top
        Framebuffer::DrawRect(x, y, 1, h, 0xFFFFFF); // highlight left
        Framebuffer::DrawRect(x + w - 1, y, 1, h, 0x000000); // shadow right
        Framebuffer::DrawRect(x, y + h - 1, w, 1, 0x000000); // shadow bottom
    }
    
    int text_len = 0;
    while(text[text_len]) text_len++;
    int text_w = text_len * 8;
    int text_x = x + (w - text_w) / 2;
    int text_y = y + (h - 16) / 2;
    
    if (pressed) {
        text_x += 1;
        text_y += 1;
    }
    
    Framebuffer::DrawString(text, text_x, text_y, 0x000000, 0xC0C0C0);
}

static void IntToString(int val, char* buf) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    int temp = val;
    if (temp < 0) temp = -temp;
    int pos = 0;
    char rev[16];
    while (temp > 0) {
        rev[pos++] = (temp % 10) + '0';
        temp /= 10;
    }
    if (val < 0) rev[pos++] = '-';
    
    int dpos = 0;
    while (pos > 0) {
        buf[dpos++] = rev[--pos];
    }
    buf[dpos] = '\0';
}

// --- Calculator App ---

void CalculatorApp::OnInit(Window* win) {
    window = win;
    current_val = 0;
    stored_val = 0;
    current_op = 0;
    new_number = true;
    display[0] = '0';
    display[1] = '\0';
}

void CalculatorApp::PlayErrorSound() {
    uint8_t* wav_buffer = nullptr;
    uint32_t wav_size = 0;
    if (VFS::ReadFile("/ERROR.WAV", &wav_buffer, &wav_size)) {
        AC97::PlayWAV(wav_buffer);
    }
}

void CalculatorApp::OnPaint(int win_x, int win_y, int width, int height) {
    // Tło kalkulatora
    Framebuffer::DrawRect(win_x, win_y, width, height, 0xC0C0C0);
    
    // Wyświetlacz
    Framebuffer::DrawRect(win_x + 10, win_y + 10, width - 20, 30, 0xFFFFFF);
    Framebuffer::DrawRect(win_x + 9, win_y + 9, width - 18, 1, 0x000000); // top shadow
    Framebuffer::DrawRect(win_x + 9, win_y + 9, 1, 32, 0x000000); // left shadow
    
    // Oblicz długość tekstu
    int len = 0;
    while(display[len]) len++;
    int text_x = win_x + width - 15 - (len * 8); // wyrównanie do prawej
    Framebuffer::DrawString(display, text_x, win_y + 17, 0x000000, 0xFFFFFF);
    
    // Rysowanie przycisków
    const char* buttons[16] = {
        "7", "8", "9", "/",
        "4", "5", "6", "*",
        "1", "2", "3", "-",
        "C", "0", "=", "+"
    };
    
    int start_y = win_y + 50;
    int bw = (width - 50) / 4;
    int bh = 30;
    
    for (int i = 0; i < 16; i++) {
        int row = i / 4;
        int col = i % 4;
        int bx = win_x + 10 + col * (bw + 10);
        int by = start_y + row * (bh + 10);
        
        DrawAppButton(bx, by, bw, bh, buttons[i], false);
    }
}

void CalculatorApp::OnMouseClick(int local_x, int local_y) {
    int width = window->width;
    int bw = (width - 50) / 4;
    int bh = 30;
    
    const char* buttons[16] = {
        "7", "8", "9", "/",
        "4", "5", "6", "*",
        "1", "2", "3", "-",
        "C", "0", "=", "+"
    };
    
    int start_y = 50;
    
    for (int i = 0; i < 16; i++) {
        int row = i / 4;
        int col = i % 4;
        int bx = 10 + col * (bw + 10);
        int by = start_y + row * (bh + 10);
        
        if (local_x >= bx && local_x <= bx + bw && local_y >= by && local_y <= by + bh) {
            char btn = buttons[i][0];
            int len = 0;
            while(display[len]) len++;
            
            if (btn >= '0' && btn <= '9') {
                if (new_number && current_op == 0) {
                    display[0] = btn;
                    display[1] = '\0';
                    current_val = btn - '0';
                    new_number = false;
                } else {
                    if (len < 30) {
                        display[len] = btn;
                        display[len+1] = '\0';
                        if (new_number) {
                            current_val = btn - '0';
                            new_number = false;
                        } else {
                            current_val = current_val * 10 + (btn - '0');
                        }
                    }
                }
            } else if (btn == 'C') {
                display[0] = '0';
                display[1] = '\0';
                current_val = 0;
                stored_val = 0;
                current_op = 0;
                new_number = true;
            } else if (btn == '+' || btn == '-' || btn == '*' || btn == '/') {
                if (current_op != 0 && !new_number) {
                    if (current_op == '+') current_val = stored_val + current_val;
                    else if (current_op == '-') current_val = stored_val - current_val;
                    else if (current_op == '*') current_val = stored_val * current_val;
                    else if (current_op == '/') {
                        if (current_val == 0) {
                            PlayErrorSound();
                            display[0] = 'E'; display[1] = 'R'; display[2] = 'R'; display[3] = '\0';
                            new_number = true;
                            current_op = 0;
                            return;
                        }
                        else current_val = stored_val / current_val;
                    }
                }
                stored_val = current_val;
                current_op = btn;
                new_number = true;
                
                IntToString(stored_val, display);
                len = 0; while(display[len]) len++;
                display[len] = btn;
                display[len+1] = '\0';
                
            } else if (btn == '=') {
                if (current_op != 0) {
                    if (current_op == '+') current_val = stored_val + current_val;
                    else if (current_op == '-') current_val = stored_val - current_val;
                    else if (current_op == '*') current_val = stored_val * current_val;
                    else if (current_op == '/') {
                        if (current_val == 0) {
                            PlayErrorSound();
                            display[0] = 'E'; display[1] = 'R'; display[2] = 'R'; display[3] = '\0';
                            new_number = true;
                            current_op = 0;
                            return;
                        }
                        else current_val = stored_val / current_val;
                    }
                    current_op = 0;
                    IntToString(current_val, display);
                    new_number = true;
                }
            }
            break;
        }
    }
}

// --- Notepad App ---

void NotepadApp::OnInit(Window* win) {
    window = win;
    for (int i = 0; i < 1024; i++) text_buffer[i] = 0;
    cursor_pos = 0;
    
    uint8_t* wav_buffer = nullptr;
    uint32_t wav_size = 0;
    if (VFS::ReadFile("/INFO.WAV", &wav_buffer, &wav_size)) {
        AC97::PlayWAV(wav_buffer);
    }
}

void NotepadApp::OnPaint(int win_x, int win_y, int width, int height) {
    Framebuffer::DrawRect(win_x, win_y, width, height, 0xFFFFFF); // Białe tło
    Framebuffer::DrawRect(win_x, win_y, width, 2, 0x000000); // Wewnętrzny cień
    Framebuffer::DrawRect(win_x, win_y, 2, height, 0x000000);
    
    // Rysowanie tekstu, uwzględniając znaki nowej linii
    int cx = win_x + 5;
    int cy = win_y + 5;
    
    for (int i = 0; i < cursor_pos; i++) {
        if (text_buffer[i] == '\n') {
            cx = win_x + 5;
            cy += 16;
        } else {
            char str[2] = {text_buffer[i], 0};
            Framebuffer::DrawString(str, cx, cy, 0x000000, 0xFFFFFF);
            cx += 8;
        }
    }
    
    // Rysowanie kursora
    Framebuffer::DrawRect(cx, cy, 8, 16, 0x000000); // Solidny czarny prostokąt jako kursor
}

void NotepadApp::OnKeyPress(char c) {
    if (c == '\b') { // Backspace
        if (cursor_pos > 0) {
            cursor_pos--;
            text_buffer[cursor_pos] = 0;
        }
    } else if (c == '\n') { // Enter
        if (cursor_pos < 1023) {
            text_buffer[cursor_pos++] = '\n';
        }
    } else if (c >= 32 && c <= 126) {
        if (cursor_pos < 1023) {
            text_buffer[cursor_pos++] = c;
        }
    }
}

// --- Paint App ---

const uint32_t paint_palette[8] = {
    0x000000, 0xFFFFFF, 0xFF0000, 0x00FF00, 
    0x0000FF, 0xFFFF00, 0x00FFFF, 0xFF00FF
};

void PaintApp::OnInit(Window* win) {
    window = win;
    current_color = 0; // Czarny
    for(int y=0; y<32; y++) {
        for(int x=0; x<32; x++) {
            canvas[x][y] = 1; // Biały domyślnie
        }
    }
}

void PaintApp::OnPaint(int win_x, int win_y, int width, int height) {
    Framebuffer::DrawRect(win_x, win_y, width, height, 0x808080); // Tło

    int cell_size = 6;
    int grid_x = win_x + (width - 32 * cell_size) / 2;
    int grid_y = win_y + 8;
    
    // Obramowanie płótna
    Framebuffer::DrawRect(grid_x - 2, grid_y - 2, 32 * cell_size + 4, 32 * cell_size + 4, 0x000000);
    Framebuffer::DrawRect(grid_x - 1, grid_y - 1, 32 * cell_size + 2, 32 * cell_size + 2, 0xFFFFFF);
    
    // Rysowanie siatki
    for(int y=0; y<32; y++) {
        for(int x=0; x<32; x++) {
            Framebuffer::DrawRect(grid_x + x * cell_size, grid_y + y * cell_size, cell_size, cell_size, paint_palette[canvas[x][y]]);
        }
    }
    
    // Rysowanie palety
    int palette_y = win_y + height - 40;
    Framebuffer::DrawRect(win_x + 4, palette_y, width - 8, 36, 0xC0C0C0);
    Framebuffer::DrawRect(win_x + 4, palette_y, width - 8, 2, 0xFFFFFF);
    Framebuffer::DrawRect(win_x + 4, palette_y, 2, 36, 0xFFFFFF);
    Framebuffer::DrawRect(win_x + width - 6, palette_y, 2, 36, 0x000000);
    Framebuffer::DrawRect(win_x + 4, palette_y + 34, width - 8, 2, 0x000000);
    
    for(int i=0; i<8; i++) {
        int px = win_x + 12 + i * 28;
        int py = palette_y + 8;
        Framebuffer::DrawRect(px, py, 20, 20, paint_palette[i]);
        if (current_color == i) {
            Framebuffer::DrawRect(px-2, py-2, 24, 2, 0x000000);
            Framebuffer::DrawRect(px-2, py-2, 2, 24, 0x000000);
            Framebuffer::DrawRect(px+22, py-2, 2, 24, 0x000000);
            Framebuffer::DrawRect(px-2, py+22, 24, 2, 0x000000);
        }
    }
}

void PaintApp::DrawPixel(int local_x, int local_y) {
    if (!window) return;
    int width = window->width;
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

void PaintApp::OnMouseClick(int local_x, int local_y) {
    if (!window) return;
    int height = window->height - 20;
    int palette_y = height - 40;
    
    if (local_y >= palette_y && local_y <= palette_y + 36) {
        for(int i=0; i<8; i++) {
            int px = 12 + i * 28;
            if (local_x >= px && local_x <= px + 20) {
                current_color = i;
                break;
            }
        }
    } else {
        DrawPixel(local_x, local_y);
    }
}

void PaintApp::OnMouseMove(int local_x, int local_y) {
    DrawPixel(local_x, local_y);
}
