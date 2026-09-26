#include <gui.h>
#include <widgets.h>
#include <stddef.h>

#define FILE_PATH "/notatka.txt"
#define LINE_HEIGHT 12
#define CHAR_WIDTH 8
#define FIRST_LINE_Y 25

int win_id;
uint32_t* fb;
int win_w = 500;
int win_h = 350;

Form form;
Control* status_label;

char text_buffer[4096];
int text_len = 0;
int scroll_offset_lines = 0;

// Symuluje ten sam zawijanie/nowa-linia co pass rysujacy, ale bez rysowania - zeby
// policzyc na ktorej linii jest kursor (koniec text_buffer) przed narysowaniem czegokolwiek.
int MeasureCursorLine(void) {
    int cx = 5;
    int line = 0;
    for (int i = 0; i < text_len; i++) {
        char c = text_buffer[i];
        if (c == '\n') {
            cx = 5;
            line++;
            continue;
        }
        if (cx > win_w - 15) {
            cx = 5;
            line++;
        }
        cx += CHAR_WIDTH;
    }
    return line;
}

static void SetStatus(const char* msg) {
    int i = 0;
    while (msg[i] && i < 63) {
        status_label->text[i] = msg[i];
        i++;
    }
    status_label->text[i] = '\0';
}

// Pasek menu to same klikalne etykiety tekstowe, bez zadnego tla/ramki - inny styl niz
// przyciski Kalkulatora/WinVer/Ustawien/Kalendarza (czwarty juz z kolei, potwierdza ze te
// style faktycznie sie roznia miedzy apkami, nie ma tu jednej wspolnej abstrakcji do wyciagniecia).
static void RenderMenuItem(Control* self, Form* f) {
    gui_form_draw_string(f, self->text, self->x + 5, self->y + 5, 0x000000, 0xC0C0C0);
}

static void RenderStatusLabel(Control* self, Form* f) {
    gui_form_draw_string(f, self->text, self->x, self->y, 0x000080, 0xC0C0C0);
}

// Cale pole tekstowe (ramka + tlo + zawijany tekst + kursor + auto-scroll) jako jedna
// kontrolka z wlasnym renderem - dokladnie ten sam wzorzec co planer notatek w Kalendarzu
// (Faza 3): apka w pelni zarzadza swoim tekstem/kursorem, klawiatura dochodzi przez
// Form::on_key (juz w widgets.c od Fazy 3), zero nowego typu kontrolki w bibliotece.
// Auto-scroll niesie tu podwojna role co w oryginale: trzyma kursor widocznym, a przy okazji
// (odkad gui_form_draw_rect/gui_draw_string przycinaja do win_h od Fazy 1) juz nie jest
// jedyna ochrona przed pisaniem poza bufor okna - ta ochrona jest teraz gwarantowana przez
// biblioteke niezaleznie od poprawnosci logiki scrolla.
static void RenderTextArea(Control* self, Form* f) {
    (void)self;
    gui_form_draw_rect(f, 2, 20, f->win_w - 4, f->win_h - 22, 0xFFFFFF);
    gui_form_draw_rect(f, 1, 19, f->win_w - 2, 1, 0x000000);
    gui_form_draw_rect(f, 1, 19, 1, f->win_h - 20, 0x000000);

    int visible_lines = (f->win_h - 22 - 5) / LINE_HEIGHT;
    if (visible_lines < 1) visible_lines = 1;
    int cursor_line = MeasureCursorLine();
    if (cursor_line - scroll_offset_lines >= visible_lines) {
        scroll_offset_lines = cursor_line - visible_lines + 1;
    }
    if (cursor_line < scroll_offset_lines) {
        scroll_offset_lines = cursor_line;
    }

    int cx = 5;
    int line = 0;
    int draw_x = cx;
    int draw_y = FIRST_LINE_Y;
    for (int i = 0; i < text_len; i++) {
        char c = text_buffer[i];
        if (c == '\n') {
            cx = 5;
            line++;
            continue;
        }

        if (cx > f->win_w - 15) {
            cx = 5;
            line++;
        }

        if (line >= scroll_offset_lines && line < scroll_offset_lines + visible_lines) {
            draw_x = cx;
            draw_y = FIRST_LINE_Y + (line - scroll_offset_lines) * LINE_HEIGHT;
            char buf[2] = {c, '\0'};
            gui_form_draw_string(f, buf, draw_x, draw_y, 0x000000, 0xFFFFFF);
        }
        cx += CHAR_WIDTH;
    }

    if (line >= scroll_offset_lines && line < scroll_offset_lines + visible_lines) {
        int cursor_y = FIRST_LINE_Y + (line - scroll_offset_lines) * LINE_HEIGHT;
        gui_form_draw_rect(f, cx, cursor_y + 2, 8, 2, 0x000000);
    }
}

void SaveFile(void) {
    if (sys_write_file(FILE_PATH, (const uint8_t*)text_buffer, (uint32_t)text_len)) {
        SetStatus("Zapisano.");
    } else {
        SetStatus("Blad zapisu!");
    }
}

void OpenFile(void) {
    int n = sys_read_file(FILE_PATH, (uint8_t*)text_buffer, sizeof(text_buffer) - 1);
    text_len = n;
    text_buffer[text_len] = '\0';
    scroll_offset_lines = 0;
    SetStatus(n > 0 ? "Wczytano." : "Brak pliku");
}

void NewFile(void) {
    text_len = 0;
    text_buffer[0] = '\0';
    scroll_offset_lines = 0;
    SetStatus("");
}

static void OnSaveClick(Control* self) { (void)self; SaveFile(); }
static void OnOpenClick(Control* self) { (void)self; OpenFile(); }
static void OnNewClick(Control* self) { (void)self; NewFile(); }

static void OnKeyPress(Form* f, char c) {
    (void)f;
    if (c == '\b') {
        if (text_len > 0) {
            text_len--;
            text_buffer[text_len] = '\0';
        }
    } else {
        if (text_len < (int)sizeof(text_buffer) - 1) {
            text_buffer[text_len] = c;
            text_len++;
            text_buffer[text_len] = '\0';
        }
    }
}

void _start() {
    win_id = sys_create_window("Bez tytulu - OxidePad", win_w, win_h, 50, 50, &fb);
    if (win_id < 0 || !fb) sys_exit();

    text_buffer[0] = '\0';

    gui_form_init(&form, win_id, fb, win_w, win_h, 0xC0C0C0);
    form.on_key = OnKeyPress;

    gui_form_add_control(&form, 5, 0, 50, 19, "Zapisz", RenderMenuItem, OnSaveClick, NULL);
    gui_form_add_control(&form, 60, 0, 55, 19, "Otworz", RenderMenuItem, OnOpenClick, NULL);
    gui_form_add_control(&form, 115, 0, 45, 19, "Nowy", RenderMenuItem, OnNewClick, NULL);
    status_label = gui_form_add_control(&form, win_w - 180, 5, 0, 0, "", RenderStatusLabel, NULL, NULL);
    gui_form_add_control(&form, 2, 20, win_w - 4, win_h - 22, NULL, RenderTextArea, NULL, NULL);

    gui_form_paint(&form);
    sys_update_window(win_id);

    gui_form_run(&form);
}
