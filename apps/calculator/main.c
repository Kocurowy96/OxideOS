#include <gui.h>
#include <stddef.h>

char display[32];
int current_val = 0;
int stored_val = 0;
char current_op = 0;
bool new_number = true;

int win_id;
uint32_t* fb;
int win_w = 200;
int win_h = 260;

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

void PaintCalculator() {
    int win_x = 0;
    int win_y = 0;
    
    gui_draw_rect(fb, win_w, win_x, win_y, win_w, win_h, 0xC0C0C0);
    
    gui_draw_rect(fb, win_w, win_x + 10, win_y + 10, win_w - 20, 30, 0xFFFFFF);
    gui_draw_rect(fb, win_w, win_x + 9, win_y + 9, win_w - 18, 1, 0x000000); 
    gui_draw_rect(fb, win_w, win_x + 9, win_y + 9, 1, 32, 0x000000); 
    
    int len = 0;
    while(display[len]) len++;
    int text_x = win_x + win_w - 15 - (len * 8); 
    gui_draw_string(fb, win_w, display, text_x, win_y + 17, 0x000000, 0xFFFFFF);
    
    const char* buttons[16] = {
        "7", "8", "9", "/",
        "4", "5", "6", "*",
        "1", "2", "3", "-",
        "C", "0", "=", "+"
    };
    
    int start_y = win_y + 50;
    int bw = (win_w - 50) / 4;
    int bh = 30;
    
    for (int i = 0; i < 16; i++) {
        int row = i / 4;
        int col = i % 4;
        int bx = win_x + 10 + col * (bw + 10);
        int by = start_y + row * (bh + 10);
        
        DrawAppButton(bx, by, bw, bh, buttons[i]);
    }
}

void OnMouseClick(int local_x, int local_y) {
    int bw = (win_w - 50) / 4;
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

void _start() {
    win_id = sys_create_window("Kalkulator", win_w, win_h, 100, 100, &fb);
    if (win_id < 0 || !fb) sys_exit();
    
    display[0] = '0';
    display[1] = '\0';
    
    PaintCalculator();
    sys_update_window(win_id);
    
    struct WindowEvent ev;
    while (1) {
        if (sys_get_event(win_id, &ev)) {
            if (ev.type == 1) { // Mouse click
                OnMouseClick(ev.x, ev.y);
                PaintCalculator();
                sys_update_window(win_id);
            }
        }
        for (volatile int i = 0; i < 10000; i++);
    }
}
