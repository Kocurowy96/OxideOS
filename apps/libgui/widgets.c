#include "widgets.h"
#include <stddef.h>
#include <stdint.h>

void gui_form_draw_rect(Form* form, int x, int y, int w, int h, uint32_t color) {
    // gui_draw_rect juz sprawdza X (kolumna po kolumnie, wzgledem win_w) - tu dopisujemy brakujace
    // przycinanie Y, zamiast zmieniac sygnature gui_draw_rect i wszystkie jej call site'y w kazdej
    // apce (szerszy zasieg zmiany, nie na Faze 1 - patrz PLAN_ui_library.md).
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (y + h > form->win_h) {
        h = form->win_h - y;
    }
    if (h <= 0 || w <= 0) return;
    gui_draw_rect(form->fb, form->win_w, x, y, w, h, color);
}

void gui_form_draw_string(Form* form, const char* str, int x, int y, uint32_t fg, uint32_t bg) {
    // gui_draw_string nie sprawdza w ogole ani X ani Y - rysujemy znak po znaku przez istniejaca
    // funkcje, ale pomijamy kazdy znak ktory wypadlby poza win_w/win_h zamiast dublowac jej petle
    // po foncie.
    int cx = x;
    int cy = y;
    char glyph[2] = {0, 0};
    while (*str) {
        if (*str == '\n') {
            cx = x;
            cy += 8;
        } else {
            if (cx >= 0 && cx + 8 <= form->win_w && cy >= 0 && cy + 8 <= form->win_h) {
                glyph[0] = *str;
                gui_draw_string(form->fb, form->win_w, glyph, cx, cy, fg, bg);
            }
            cx += 8;
        }
        str++;
    }
}

static void ButtonRender(Control* self, Form* form) {
    gui_form_draw_rect(form, self->x, self->y, self->w, self->h, 0x808080);
    gui_form_draw_rect(form, self->x, self->y, self->w, 2, 0xFFFFFF);
    gui_form_draw_rect(form, self->x, self->y, 2, self->h, 0xFFFFFF);
    gui_form_draw_rect(form, self->x + self->w - 2, self->y, 2, self->h, 0x000000);
    gui_form_draw_rect(form, self->x, self->y + self->h - 2, self->w, 2, 0x000000);

    int len = 0;
    while (self->text[len]) len++;

    int tx = self->x + (self->w - len * 8) / 2;
    int ty = self->y + (self->h - 8) / 2;
    gui_form_draw_string(form, self->text, tx, ty, 0x000000, 0x808080);
}

static void LabelRender(Control* self, Form* form) {
    gui_form_draw_string(form, self->text, self->x, self->y, 0x000000, 0xFFFFFFFF);
}

static void TabRender(Control* self, Form* form) {
    int tab_id = (int)(intptr_t)self->user_data;
    uint32_t bg = (tab_id == form->active_tab) ? 0x34495E : 0x2C3E50;
    gui_form_draw_rect(form, self->x, self->y, self->w, self->h, bg);
    gui_form_draw_string(form, self->text, self->x + 20, self->y + (self->h - 8) / 2, 0xFFFFFF, bg);
}

void gui_form_init(Form* form, int win_id, uint32_t* fb, int win_w, int win_h, uint32_t bg_color) {
    form->win_id = win_id;
    form->fb = fb;
    form->win_w = win_w;
    form->win_h = win_h;
    form->bg_color = bg_color;
    form->control_count = 0;
    form->on_tick = NULL;
    form->active_tab = 0;
    form->on_key = NULL;
}

Control* gui_form_add_control(Form* form, int x, int y, int w, int h, const char* text,
                               ControlRenderFn render, ControlClickFn on_click, void* user_data) {
    if (form->control_count >= FORM_MAX_CONTROLS) return NULL;

    Control* c = &form->controls[form->control_count++];
    c->x = x;
    c->y = y;
    c->w = w;
    c->h = h;
    int i = 0;
    while (text && text[i] && i < 63) {
        c->text[i] = text[i];
        i++;
    }
    c->text[i] = '\0';
    c->render = render;
    c->on_click = on_click;
    c->user_data = user_data;
    return c;
}

Control* gui_form_add_button(Form* form, int x, int y, int w, int h, const char* text,
                              ControlClickFn on_click, void* user_data) {
    return gui_form_add_control(form, x, y, w, h, text, ButtonRender, on_click, user_data);
}

Control* gui_form_add_label(Form* form, int x, int y, const char* text) {
    return gui_form_add_control(form, x, y, 0, 0, text, LabelRender, NULL, NULL);
}

Control* gui_form_add_tab(Form* form, int x, int y, int w, int h, const char* text, int tab_id,
                           ControlClickFn on_click) {
    return gui_form_add_control(form, x, y, w, h, text, TabRender, on_click, (void*)(intptr_t)tab_id);
}

void gui_form_paint(Form* form) {
    gui_form_draw_rect(form, 0, 0, form->win_w, form->win_h, form->bg_color);
    for (int i = 0; i < form->control_count; i++) {
        Control* c = &form->controls[i];
        if (c->render) c->render(c, form);
    }
}

void gui_form_run(Form* form) {
    struct WindowEvent ev;
    while (1) {
        if (form->on_tick) form->on_tick(form);

        if (sys_get_event(form->win_id, &ev)) {
            if (ev.type == GUI_EVENT_MOUSE_CLICK) {
                for (int i = 0; i < form->control_count; i++) {
                    Control* c = &form->controls[i];
                    if (c->on_click && ev.x >= c->x && ev.x <= c->x + c->w &&
                        ev.y >= c->y && ev.y <= c->y + c->h) {
                        c->on_click(c);
                        break;
                    }
                }
                gui_form_paint(form);
                sys_update_window(form->win_id);
            } else if (ev.type == GUI_EVENT_KEY_PRESS) {
                if (form->on_key) form->on_key(form, ev.key);
                gui_form_paint(form);
                sys_update_window(form->win_id);
            } else if (ev.type == GUI_EVENT_CLOSE) {
                sys_exit();
            }
        }
        for (volatile int i = 0; i < 10000; i++);
    }
}
