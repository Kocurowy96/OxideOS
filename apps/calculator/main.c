#include <gui.h>
#include <widgets.h>
#include <stddef.h>
#include <stdbool.h>

Form form;
Control* display_ctrl;

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

void RenderDisplay(Control* self, Form* f) {
    gui_form_draw_rect(f, 10, 10, f->win_w - 20, 30, 0xFFFFFF);
    gui_form_draw_rect(f, 9, 9, f->win_w - 18, 1, 0x000000);
    gui_form_draw_rect(f, 9, 9, 1, 32, 0x000000);

    int len = 0;
    while (self->text[len]) len++;
    int text_x = f->win_w - 15 - (len * 8);
    gui_form_draw_string(f, self->text, text_x, 17, 0x000000, 0xFFFFFF);
}

void OnButtonClick(Control* self) {
    char btn = self->text[0];
    int len = 0;
    while (display_ctrl->text[len]) len++;

    if (btn >= '0' && btn <= '9') {
        if (new_number) {
            display_ctrl->text[0] = btn;
            display_ctrl->text[1] = '\0';
            current_val = btn - '0';
            new_number = false;
        } else {
            if (len < 21) {
                display_ctrl->text[len] = btn;
                display_ctrl->text[len + 1] = '\0';
                current_val = current_val * 10 + (btn - '0');
            }
        }
    } else if (btn == 'C') {
        display_ctrl->text[0] = '0';
        display_ctrl->text[1] = '\0';
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
                    display_ctrl->text[0] = 'E'; display_ctrl->text[1] = 'R';
                    display_ctrl->text[2] = 'R'; display_ctrl->text[3] = '\0';
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

        IntToString(stored_val, display_ctrl->text);
        len = 0; while (display_ctrl->text[len]) len++;
        display_ctrl->text[len] = btn;
        display_ctrl->text[len + 1] = '\0';

    } else if (btn == '=') {
        if (current_op != 0) {
            if (current_op == '+') current_val = stored_val + current_val;
            else if (current_op == '-') current_val = stored_val - current_val;
            else if (current_op == '*') current_val = stored_val * current_val;
            else if (current_op == '/') {
                if (current_val == 0) {
                    display_ctrl->text[0] = 'E'; display_ctrl->text[1] = 'R';
                    display_ctrl->text[2] = 'R'; display_ctrl->text[3] = '\0';
                    new_number = true;
                    current_op = 0;
                    return;
                }
                else current_val = stored_val / current_val;
            }
            current_op = 0;
            IntToString(current_val, display_ctrl->text);
            new_number = true;
        }
    }
}

void _start() {
    uint32_t* fb;
    int win_w = 200;
    int win_h = 260;

    int win_id = sys_create_window("Kalkulator", win_w, win_h, 100, 100, &fb);
    if (win_id < 0 || !fb) sys_exit();

    gui_form_init(&form, win_id, fb, win_w, win_h, 0xC0C0C0);

    display_ctrl = gui_form_add_control(&form, 10, 10, win_w - 20, 30, "0", RenderDisplay, NULL, NULL);

    const char* buttons[16] = {
        "7", "8", "9", "/",
        "4", "5", "6", "*",
        "1", "2", "3", "-",
        "C", "0", "=", "+"
    };

    int start_y = 50;
    int bw = (win_w - 50) / 4;
    int bh = 30;

    for (int i = 0; i < 16; i++) {
        int row = i / 4;
        int col = i % 4;
        int bx = 10 + col * (bw + 10);
        int by = start_y + row * (bh + 10);

        gui_form_add_button(&form, bx, by, bw, bh, buttons[i], OnButtonClick, NULL);
    }

    gui_form_paint(&form);
    sys_update_window(win_id);

    gui_form_run(&form);
}
