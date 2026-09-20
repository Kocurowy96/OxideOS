#include <gui.h>
#include <stddef.h>
#include <stdbool.h>

#define FILE_PATH "/notatka.txt"
#define LINE_HEIGHT 12
#define CHAR_WIDTH 8
#define FIRST_LINE_Y 25

int win_id;
uint32_t* fb;
int win_w = 500;
int win_h = 350;

char text_buffer[4096];
int text_len = 0;

char status_msg[32] = "";
int scroll_offset_lines = 0;

void SetStatus(const char* msg) {
    int i = 0;
    while (msg[i] && i < (int)sizeof(status_msg) - 1) {
        status_msg[i] = msg[i];
        i++;
    }
    status_msg[i] = '\0';
}

// Symuluje ten sam zawijanie/nowa-linia co pass rysujacy, ale bez rysowania - zeby
// policzyc na ktorej linii jest kursor (koniec text_buffer) przed narysowaniem czegokolwiek.
int MeasureCursorLine() {
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

void PaintNotepad() {
    // Menu bar / Background
    gui_draw_rect(fb, win_w, 0, 0, win_w, win_h, 0xC0C0C0);

    // Menu - realne przyciski (regiony klikniec w HandleClick nizej)
    gui_draw_string(fb, win_w, "Zapisz", 10, 5, 0x000000, 0xC0C0C0);
    gui_draw_string(fb, win_w, "Otworz", 65, 5, 0x000000, 0xC0C0C0);
    gui_draw_string(fb, win_w, "Nowy", 120, 5, 0x000000, 0xC0C0C0);
    gui_draw_string(fb, win_w, status_msg, win_w - 180, 5, 0x000080, 0xC0C0C0);

    // Text area
    gui_draw_rect(fb, win_w, 2, 20, win_w - 4, win_h - 22, 0xFFFFFF);
    gui_draw_rect(fb, win_w, 1, 19, win_w - 2, 1, 0x000000); // top inner
    gui_draw_rect(fb, win_w, 1, 19, 1, win_h - 20, 0x000000); // left inner

    // Auto-scroll: trzymaj kursor zawsze widoczny w obszarze tekstu. To jest tez jedyna
    // ochrona przed rysowaniem poza win_h - gui_draw_string nie sprawdza granic w Y,
    // wiec bez tego dlugi tekst pisalby poza bufor okna.
    int visible_lines = (win_h - 22 - 5) / LINE_HEIGHT;
    if (visible_lines < 1) visible_lines = 1;
    int cursor_line = MeasureCursorLine();
    if (cursor_line - scroll_offset_lines >= visible_lines) {
        scroll_offset_lines = cursor_line - visible_lines + 1;
    }
    if (cursor_line < scroll_offset_lines) {
        scroll_offset_lines = cursor_line;
    }

    // Rysowanie tekstu z uwzglednieniem scroll_offset_lines
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

        if (cx > win_w - 15) {
            cx = 5;
            line++;
        }

        if (line >= scroll_offset_lines && line < scroll_offset_lines + visible_lines) {
            draw_x = cx;
            draw_y = FIRST_LINE_Y + (line - scroll_offset_lines) * LINE_HEIGHT;
            char buf[2] = {c, '\0'};
            gui_draw_string(fb, win_w, buf, draw_x, draw_y, 0x000000, 0xFFFFFF);
        }
        cx += CHAR_WIDTH;
    }

    // Kursor - na pozycji ostatniego znaku (cx/line z powyzszej petli)
    if (line >= scroll_offset_lines && line < scroll_offset_lines + visible_lines) {
        int cursor_y = FIRST_LINE_Y + (line - scroll_offset_lines) * LINE_HEIGHT;
        gui_draw_rect(fb, win_w, cx, cursor_y + 2, 8, 2, 0x000000);
    }
}

void SaveFile() {
    if (sys_write_file(FILE_PATH, (const uint8_t*)text_buffer, (uint32_t)text_len)) {
        SetStatus("Zapisano.");
    } else {
        SetStatus("Blad zapisu!");
    }
}

void OpenFile() {
    int n = sys_read_file(FILE_PATH, (uint8_t*)text_buffer, sizeof(text_buffer) - 1);
    text_len = n;
    text_buffer[text_len] = '\0';
    scroll_offset_lines = 0;
    SetStatus(n > 0 ? "Wczytano." : "Brak pliku");
}

void NewFile() {
    text_len = 0;
    text_buffer[0] = '\0';
    scroll_offset_lines = 0;
    SetStatus("");
}

// Zwraca true jesli klik trafil w przycisk menu (i go obsluzyl)
bool HandleMenuClick(int x, int y) {
    if (y < 0 || y > 18) return false;
    if (x >= 5 && x <= 55) { SaveFile(); return true; }
    if (x >= 60 && x <= 115) { OpenFile(); return true; }
    if (x >= 118 && x <= 160) { NewFile(); return true; }
    return false;
}

void _start() {
    win_id = sys_create_window("Bez tytulu - OxidePad", win_w, win_h, 50, 50, &fb);
    if (win_id < 0 || !fb) sys_exit();

    text_buffer[0] = '\0';

    PaintNotepad();
    sys_update_window(win_id);

    struct WindowEvent ev;
    while (1) {
        if (sys_get_event(win_id, &ev)) {
            bool dirty = false;
            if (ev.type == GUI_EVENT_MOUSE_CLICK) {
                if (HandleMenuClick(ev.x, ev.y)) dirty = true;
            } else if (ev.type == GUI_EVENT_KEY_PRESS) {
                char c = ev.key;
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
                dirty = true;
            }
            if (dirty) {
                PaintNotepad();
                sys_update_window(win_id);
            }
        }
        for (volatile int i = 0; i < 10000; i++);
    }
}
