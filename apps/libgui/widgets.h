#pragma once
#include <gui.h>

typedef struct Control Control;
typedef struct Form Form;

typedef void (*ControlRenderFn)(Control* self, Form* form);
typedef void (*ControlClickFn)(Control* self);
typedef void (*FormTickFn)(Form* form);
typedef void (*FormKeyFn)(Form* form, char key);

struct Control {
    int x, y, w, h;
    char text[64];
    ControlRenderFn render;   // rysuje siebie (tlo/ramka/tekst)
    ControlClickFn on_click;  // wolane przez Form po trafieniu kliku w (x,y,w,h), moze byc NULL
    void* user_data;          // wskaznik do stanu apki
};

// 32 wystarczalo Kalkulatorowi/WinVer/Ustawieniom, ale siatka dni Kalendarza (do 31 komorek)
// plus stale kontrolki naglowka/planera przekracza to - podniesione do 40 (Faza 3).
#define FORM_MAX_CONTROLS 40

struct Form {
    int win_id;
    uint32_t* fb;
    int win_w, win_h;
    uint32_t bg_color;
    Control controls[FORM_MAX_CONTROLS];
    int control_count;
    FormTickFn on_tick;  // wolane raz na kazdy przebieg petli gui_form_run, niezaleznie od zdarzen -
                         // do stanu ktory zmienia sie sam (np. odswiezanie RAM/zegara w tle). Moze
                         // byc NULL (domyslnie po gui_form_init) - apka bez takiej potrzeby nic nie robi.
    int active_tab;      // aktualnie wybrana zakladka dla kontrolek dodanych przez gui_form_add_tab -
                         // apka sama go ustawia (np. w on_click zakladki), kontrolki inne niz zakladki
                         // go ignoruja.
    FormKeyFn on_key;    // wolane przez gui_form_run przy GUI_EVENT_KEY_PRESS, moze byc NULL (domyslnie
                         // po gui_form_init) - apka sama zarzadza swoim tekstem/kursorem, biblioteka nie
                         // ma jeszcze wlasnej kontrolki pola tekstowego (patrz PLAN_ui_library.md, do
                         // zaprojektowania osobno przy Notatniku).
};

void gui_form_init(Form* form, int win_id, uint32_t* fb, int win_w, int win_h, uint32_t bg_color);

Control* gui_form_add_control(Form* form, int x, int y, int w, int h, const char* text,
                               ControlRenderFn render, ControlClickFn on_click, void* user_data);
Control* gui_form_add_button(Form* form, int x, int y, int w, int h, const char* text,
                              ControlClickFn on_click, void* user_data);
Control* gui_form_add_label(Form* form, int x, int y, const char* text);
// Pozycja na pasku zakladek/sidebarze - podswietla sie sama, gdy form->active_tab == tab_id
// (apka ustawia form->active_tab w on_click). Styl na sztywno Win95-sidebar (ciemnoniebieski,
// bialy tekst) - jedyny uzytkownik na razie to Ustawienia, dopisac parametryzacje kolorow
// dopiero gdy pojawi sie druga apka z inna paleta zakladek.
Control* gui_form_add_tab(Form* form, int x, int y, int w, int h, const char* text, int tab_id,
                           ControlClickFn on_click);

void gui_form_paint(Form* form);   // czysci tlo (bg_color) + rysuje wszystkie kontrolki po kolei
void gui_form_run(Form* form);     // petla: sys_get_event -> hit-test -> on_click -> gui_form_paint -> sys_update_window

// Odpowiedniki gui_draw_rect/gui_draw_string, ale przycinane do win_w/win_h danego Form - zeby
// kontrolka (wbudowana albo wlasny ControlRenderFn apki) nie mogla zapisac poza swoim oknem.
// gui_draw_rect/gui_draw_string same tego nie robia (brak per-process page tables - przekroczenie
// bufora okna nadpisuje cudza pamiec, nie tylko psuje obraz), patrz PLAN_ui_library.md.
void gui_form_draw_rect(Form* form, int x, int y, int w, int h, uint32_t color);
void gui_form_draw_string(Form* form, const char* str, int x, int y, uint32_t fg, uint32_t bg);
