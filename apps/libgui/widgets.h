#pragma once
#include <gui.h>

typedef struct Control Control;
typedef struct Form Form;

typedef void (*ControlRenderFn)(Control* self, Form* form);
typedef void (*ControlClickFn)(Control* self);

struct Control {
    int x, y, w, h;
    char text[64];
    ControlRenderFn render;   // rysuje siebie (tlo/ramka/tekst)
    ControlClickFn on_click;  // wolane przez Form po trafieniu kliku w (x,y,w,h), moze byc NULL
    void* user_data;          // wskaznik do stanu apki
};

#define FORM_MAX_CONTROLS 32

struct Form {
    int win_id;
    uint32_t* fb;
    int win_w, win_h;
    uint32_t bg_color;
    Control controls[FORM_MAX_CONTROLS];
    int control_count;
};

void gui_form_init(Form* form, int win_id, uint32_t* fb, int win_w, int win_h, uint32_t bg_color);

Control* gui_form_add_control(Form* form, int x, int y, int w, int h, const char* text,
                               ControlRenderFn render, ControlClickFn on_click, void* user_data);
Control* gui_form_add_button(Form* form, int x, int y, int w, int h, const char* text,
                              ControlClickFn on_click, void* user_data);
Control* gui_form_add_label(Form* form, int x, int y, const char* text);

void gui_form_paint(Form* form);   // czysci tlo (bg_color) + rysuje wszystkie kontrolki po kolei
void gui_form_run(Form* form);     // petla: sys_get_event -> hit-test -> on_click -> gui_form_paint -> sys_update_window

// Odpowiedniki gui_draw_rect/gui_draw_string, ale przycinane do win_w/win_h danego Form - zeby
// kontrolka (wbudowana albo wlasny ControlRenderFn apki) nie mogla zapisac poza swoim oknem.
// gui_draw_rect/gui_draw_string same tego nie robia (brak per-process page tables - przekroczenie
// bufora okna nadpisuje cudza pamiec, nie tylko psuje obraz), patrz PLAN_ui_library.md.
void gui_form_draw_rect(Form* form, int x, int y, int w, int h, uint32_t color);
void gui_form_draw_string(Form* form, const char* str, int x, int y, uint32_t fg, uint32_t bg);
