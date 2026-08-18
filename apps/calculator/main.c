#include <gui.h>
#include <stddef.h>

char display[32];
int current_val = 0;
int stored_val = 0;
char current_op = 0;
bool new_number = true;

void IntToString(int val, char* str) {
    if (val == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }
    int temp = val;
    int len = 0;
    if (temp < 0) {
        len++;
        temp = -temp;
    }
    while (temp > 0) {
        len++;
        temp /= 10;
    }
    str[len] = '\0';
    temp = val;
    if (temp < 0) {
        str[0] = '-';
        temp = -temp;
    }
    for (int i = len - 1; i >= (val < 0 ? 1 : 0); i--) {
        str[i] = (temp % 10) + '0';
        temp /= 10;
    }
}

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

void PaintCalculator(Window* win) {
    int win_x = 0;
    int win_y = 0;
    int width = 200;
    int height = 260;
    
    // Tło kalkulatora
    gui_draw_rect(win, win_x, win_y, width, height, 0xC0C0C0);
    
    // Wyświetlacz
    gui_draw_rect(win, win_x + 10, win_y + 10, width - 20, 30, 0xFFFFFF);
    gui_draw_rect(win, win_x + 9, win_y + 9, width - 18, 1, 0x000000); // top shadow
    gui_draw_rect(win, win_x + 9, win_y + 9, 1, 32, 0x000000); // left shadow
    
    // Oblicz długość tekstu
    int len = 0;
    while(display[len]) len++;
    int text_x = win_x + width - 15 - (len * 8); // wyrównanie do prawej
    gui_draw_string(win, display, text_x, win_y + 17, 0x000000, 0xFFFFFF);
    
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
        
        DrawAppButton(win, bx, by, bw, bh, buttons[i]);
    }
}

void OnMouseClick(Window* win, int local_x, int local_y) {
    int width = 200;
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

int main() {
    Window* win = sys_create_window(100, 100, 200, 260, "Kalkulator");
    if (!win) return 1;
    
    display[0] = '0';
    display[1] = '\0';
    
    PaintCalculator(win);
    sys_update_window(win);
    
    Event ev;
    while (1) {
        if (sys_get_event(win, &ev)) {
            if (ev.type == EventWindowClose) {
                break;
            } else if (ev.type == EventMouseClick) {
                OnMouseClick(win, ev.mouse_x, ev.mouse_y);
                PaintCalculator(win);
                sys_update_window(win);
            }
        }
    }
    
    sys_exit();
    return 0;
}
