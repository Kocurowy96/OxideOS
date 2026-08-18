#include "apps.h"
#include "window.h"
#include "fb.h"
#include "../drivers/ac97.h"
#include "../fs/vfs.h"
#include "../drivers/rtc.h"
#include "../mem/pmm.h"
#include "../limine.h"

extern volatile struct limine_hhdm_request hhdm_request;
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

// --- Welcome App ---
void WelcomeApp::OnInit(Window* win) {
    window = win;
}

void WelcomeApp::OnPaint(int win_x, int win_y, int width, int height) {
    Framebuffer::DrawRect(win_x, win_y, width, height, 0xFFFFFF); // Tło białe
    
    Framebuffer::DrawString("Witaj w OxideOS!", win_x + 10, win_y + 10, 0x000000, 0xFFFFFF);
    Framebuffer::DrawString("Wersja kernela: 1.0 (Ring 3)", win_x + 10, win_y + 30, 0x000080, 0xFFFFFF);
    
    Framebuffer::DrawString("Nowosci:", win_x + 10, win_y + 60, 0x000000, 0xFFFFFF);
    Framebuffer::DrawString("- Aplikacje w trybie Userspace", win_x + 20, win_y + 80, 0x000000, 0xFFFFFF);
    Framebuffer::DrawString("- Aplikacja Ustawien w menu start", win_x + 20, win_y + 100, 0x000000, 0xFFFFFF);
    Framebuffer::DrawString("- Dzwiek AC97", win_x + 20, win_y + 120, 0x000000, 0xFFFFFF);
    
    Framebuffer::DrawString("Milego korzystania ze swiezego systemu!", win_x + 10, win_y + 160, 0x008000, 0xFFFFFF);
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
                if (new_number) {
                    display[0] = btn;
                    display[1] = '\0';
                    current_val = btn - '0';
                    new_number = false;
                } else {
                    if (len < 21) {
                        display[len] = btn;
                        display[len+1] = '\0';
                        current_val = current_val * 10 + (btn - '0');
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
    
    // Rysowanie panelu narzędzi (60 px wysokości)
    int palette_y = win_y + height - 60;
    Framebuffer::DrawRect(win_x + 4, palette_y, width - 8, 56, 0xC0C0C0);
    Framebuffer::DrawRect(win_x + 4, palette_y, width - 8, 2, 0xFFFFFF);
    Framebuffer::DrawRect(win_x + 4, palette_y, 2, 56, 0xFFFFFF);
    Framebuffer::DrawRect(win_x + width - 6, palette_y, 2, 56, 0x000000);
    Framebuffer::DrawRect(win_x + 4, palette_y + 54, width - 8, 2, 0x000000);
    
    // Paleta barw (pierwszy rząd)
    for(int i=0; i<8; i++) {
        int px = win_x + 12 + i * 28;
        int py = palette_y + 6;
        Framebuffer::DrawRect(px, py, 20, 20, paint_palette[i]);
        if (current_color == i) {
            Framebuffer::DrawRect(px-2, py-2, 24, 2, 0x000000);
            Framebuffer::DrawRect(px-2, py-2, 2, 24, 0x000000);
            Framebuffer::DrawRect(px+22, py-2, 2, 24, 0x000000);
            Framebuffer::DrawRect(px-2, py+22, 24, 2, 0x000000);
        }
    }
    
    // Rysowanie przycisków
    int btn_py = palette_y + 32;
    // Gumka (traktowana jako kolor 1, ale z UI ułatwieniem)
    Framebuffer::DrawRect(win_x + 12, btn_py, 60, 20, 0x808080); // Cień dla wklęsłości by wyglądało jak przycisk narzędziowy
    Framebuffer::DrawString("GUMKA", win_x + 16, btn_py + 4, 0x000000, 0x808080);
    if (current_color == 1) { // Podświetl jeśli wybrana gumka (biały)
        Framebuffer::DrawRect(win_x + 10, btn_py - 2, 64, 2, 0x000000);
        Framebuffer::DrawRect(win_x + 10, btn_py - 2, 2, 24, 0x000000);
        Framebuffer::DrawRect(win_x + 72, btn_py - 2, 2, 24, 0x000000);
        Framebuffer::DrawRect(win_x + 10, btn_py + 20, 64, 2, 0x000000);
    }
    
    // CZYŚĆ
    Framebuffer::DrawRect(win_x + 80, btn_py, 70, 20, 0x808080);
    Framebuffer::DrawString("CZYSC", win_x + 84, btn_py + 4, 0x000000, 0x808080);
    
    // ZAPISZ
    Framebuffer::DrawRect(win_x + 160, btn_py, 70, 20, 0x808080);
    Framebuffer::DrawString("ZAPISZ", win_x + 164, btn_py + 4, 0x000000, 0x808080);
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
                // ZAPISZ
                // Generowanie pliku BMP (naglowek 54 bajty + piksele)
                int bmp_size = 54 + 32 * 32 * 3;
                uint8_t* bmp = (uint8_t*)((uint64_t)PMM::AllocatePage() + hhdm_request.response->offset);
                if (bmp) {
                    for (int i=0; i<bmp_size; i++) bmp[i] = 0;
                    bmp[0] = 'B'; bmp[1] = 'M';
                    *(uint32_t*)&bmp[2] = bmp_size;
                    *(uint32_t*)&bmp[10] = 54;
                    *(uint32_t*)&bmp[14] = 40;
                    *(uint32_t*)&bmp[18] = 32;
                    *(uint32_t*)&bmp[22] = 32;
                    *(uint16_t*)&bmp[26] = 1;
                    *(uint16_t*)&bmp[28] = 24;
                    
                    int idx = 54;
                    for (int y=31; y>=0; y--) {
                        for (int x=0; x<32; x++) {
                            uint32_t col = paint_palette[canvas[x][y]];
                            bmp[idx++] = col & 0xFF; // B
                            bmp[idx++] = (col >> 8) & 0xFF; // G
                            bmp[idx++] = (col >> 16) & 0xFF; // R
                        }
                    }
                    VFS::WriteFile("PICS/DRAW.BMP", bmp, bmp_size);
                }
            }
        }
    } else {
        DrawPixel(local_x, local_y);
    }
}

void PaintApp::OnMouseMove(int local_x, int local_y) {
    DrawPixel(local_x, local_y);
}

// --- Calendar App ---

static int BcdToBin(uint8_t bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

int CalendarApp::GetDaysInMonth(int m, int y) {
    if (m == 2) {
        if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) return 29;
        return 28;
    }
    if (m == 4 || m == 6 || m == 9 || m == 11) return 30;
    return 31;
}

int CalendarApp::GetDayOfWeek(int d, int m, int y) {
    if (m < 3) {
        m += 12;
        y -= 1;
    }
    int K = y % 100;
    int J = y / 100;
    int h = (d + (13 * (m + 1)) / 5 + K + (K / 4) + (J / 4) - 2 * J) % 7;
    return (h + 5) % 7; 
}

void CalendarApp::OnInit(Window* win) {
    window = win;
    current_year = 2000 + BcdToBin(RTC::GetYear());
    current_month = BcdToBin(RTC::GetMonth());
    selected_day = BcdToBin(RTC::GetDay());
    if(current_month < 1 || current_month > 12) current_month = 1;
    if(selected_day < 1 || selected_day > 31) selected_day = 1;
    
    cursor_pos = 0;
    for(int i=0; i<31; i++) {
        for(int j=0; j<128; j++) notes[i][j] = '\0';
    }
    
    uint8_t* buf = nullptr;
    uint32_t size = 0;
    if (VFS::ReadFile("DOCS/NOTES.DAT", &buf, &size) && buf && size == sizeof(notes)) {
        for(int i=0; i<31; i++) {
            for(int j=0; j<128; j++) notes[i][j] = buf[i*128 + j];
        }
    }
}

void CalendarApp::SaveNotes() {
    VFS::WriteFile("DOCS/NOTES.DAT", (const uint8_t*)notes, sizeof(notes));
}

void CalendarApp::OnPaint(int win_x, int win_y, int width, int height) {
    Framebuffer::DrawRect(win_x, win_y, width, height, 0xFFFFFF); // Tło
    
    // Top banner
    Framebuffer::DrawRect(win_x, win_y, width, 30, 0x000080);
    Framebuffer::DrawRect(win_x + 5, win_y + 5, 20, 20, 0xC0C0C0);
    Framebuffer::DrawString("<", win_x + 11, win_y + 11, 0x000000, 0xC0C0C0);
    
    Framebuffer::DrawRect(win_x + width - 25, win_y + 5, 20, 20, 0xC0C0C0);
    Framebuffer::DrawString(">", win_x + width - 19, win_y + 11, 0x000000, 0xC0C0C0);
    
    char m_str[16];
    char y_str[16];
    IntToString(current_month, m_str);
    IntToString(current_year, y_str);
    
    Framebuffer::DrawString("Miesiac:", win_x + 35, win_y + 11, 0xFFFFFF, 0x000080);
    Framebuffer::DrawString(m_str, win_x + 105, win_y + 11, 0xFFFFFF, 0x000080);
    Framebuffer::DrawString("Rok:", win_x + 135, win_y + 11, 0xFFFFFF, 0x000080);
    Framebuffer::DrawString(y_str, win_x + 175, win_y + 11, 0xFFFFFF, 0x000080);
    
    // Siatka
    int start_day = GetDayOfWeek(1, current_month, current_year);
    int days = GetDaysInMonth(current_month, current_year);
    
    int cell_w = 30;
    int cell_h = 20;
    int grid_x = win_x + (width - 7 * cell_w) / 2;
    int grid_y = win_y + 50;
    
    for (int i = 0; i < days; i++) {
        int col = (start_day + i) % 7;
        int row = (start_day + i) / 7;
        
        int px = grid_x + col * cell_w;
        int py = grid_y + row * cell_h;
        
        uint32_t bg = (i + 1 == selected_day) ? 0x808080 : 0xC0C0C0;
        Framebuffer::DrawRect(px, py, cell_w - 2, cell_h - 2, bg);
        
        char d_str[16];
        IntToString(i + 1, d_str);
        Framebuffer::DrawString(d_str, px + 2, py + 4, 0x000000, bg);
    }
    
    // Planner
    int planner_y = grid_y + 6 * cell_h + 10;
    int planner_h = height - (planner_y - win_y) - 5;
    Framebuffer::DrawRect(win_x + 5, planner_y, width - 10, planner_h, 0xFFFFCC); 
    
    if (selected_day >= 1 && selected_day <= 31) {
        char* note = notes[selected_day - 1];
        int cx = win_x + 10;
        int cy = planner_y + 10;
        for (int i = 0; note[i] != '\0'; i++) {
            if (note[i] == '\n') {
                cx = win_x + 10;
                cy += 16;
            } else {
                char str[2] = {note[i], 0};
                Framebuffer::DrawString(str, cx, cy, 0x000000, 0xFFFFCC);
                cx += 8;
            }
        }
        Framebuffer::DrawRect(cx, cy, 8, 16, 0x000000); 
    }
}

void CalendarApp::OnMouseClick(int local_x, int local_y) {
    if (!window) return;
    if (local_y >= 5 && local_y <= 25) {
        if (local_x >= 5 && local_x <= 25) {
            current_month--;
            if (current_month < 1) { current_month = 12; current_year--; }
            selected_day = 1;
            cursor_pos = 0;
            while(notes[selected_day-1][cursor_pos]) cursor_pos++;
        }
        if (local_x >= window->width - 25 && local_x <= window->width - 5) {
            current_month++;
            if (current_month > 12) { current_month = 1; current_year++; }
            selected_day = 1;
            cursor_pos = 0;
            while(notes[selected_day-1][cursor_pos]) cursor_pos++;
        }
    }
    
    int cell_w = 30;
    int cell_h = 20;
    int grid_x = (window->width - 7 * cell_w) / 2;
    int grid_y = 50;
    
    int start_day = GetDayOfWeek(1, current_month, current_year);
    int days = GetDaysInMonth(current_month, current_year);
    
    for (int i = 0; i < days; i++) {
        int col = (start_day + i) % 7;
        int row = (start_day + i) / 7;
        int px = grid_x + col * cell_w;
        int py = grid_y + row * cell_h;
        
        if (local_x >= px && local_x <= px + cell_w - 2 &&
            local_y >= py && local_y <= py + cell_h - 2) {
            selected_day = i + 1;
            cursor_pos = 0;
            while(notes[selected_day-1][cursor_pos]) cursor_pos++;
        }
    }
}

void CalendarApp::OnKeyPress(char c) {
    if (selected_day < 1 || selected_day > 31) return;
    char* note = notes[selected_day - 1];
    
    if (c == '\b') {
        if (cursor_pos > 0) {
            cursor_pos--;
            note[cursor_pos] = 0;
            SaveNotes();
        }
    } else if (c == '\n') {
        if (cursor_pos < 127) {
            note[cursor_pos++] = '\n';
            note[cursor_pos] = 0;
            SaveNotes();
        }
    } else if (c >= 32 && c <= 126) {
        if (cursor_pos < 127) {
            note[cursor_pos++] = c;
            note[cursor_pos] = 0;
            SaveNotes();
        }
    }
}
