#include <gui.h>
#include <widgets.h>
#include <stddef.h>
#include <stdint.h>

int win_id;
uint32_t* fb;
int win_w = 260;
int win_h = 360;

Form form;
Control* month_label;
Control* year_label;

int current_month = 1;
int current_year = 2000;
int selected_day = 1;

char notes[31][128];
int cursor_pos = 0;

// Ilosc stalych kontrolek (naglowek + planer notatek) dodanych raz w _start(), przed siatka
// dni. RebuildDayGrid() obcina Form::control_count z powrotem do tej wartosci przy kazdej
// zmianie miesiaca, zeby dodac na nowo tylko komorki wlasciwe nowemu miesiacowi - ten sam
// wzorzec co RebuildContent() w Ustawieniach (Faza 2).
#define GRID_START_INDEX 8

void IntToString(int val, char* str) {
    if (val == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }
    int temp = val;
    int len = 0;
    while (temp > 0) {
        len++;
        temp /= 10;
    }
    str[len] = '\0';
    temp = val;
    for (int i = len - 1; i >= 0; i--) {
        str[i] = (temp % 10) + '0';
        temp /= 10;
    }
}

int GetDaysInMonth(int m, int y) {
    if (m == 2) {
        if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) return 29;
        return 28;
    }
    if (m == 4 || m == 6 || m == 9 || m == 11) return 30;
    return 31;
}

int GetDayOfWeek(int d, int m, int y) {
    if (m < 3) {
        m += 12;
        y -= 1;
    }
    int K = y % 100;
    int J = y / 100;
    int h = (d + (13 * (m + 1)) / 5 + K + (K / 4) + (J / 4) - 2 * J) % 7;
    return (h + 5) % 7;
}

void SaveNotes(void) {
    sys_write_file("/NOTES.DAT", (const uint8_t*)notes, sizeof(notes));
}

void LoadNotes(void) {
    sys_read_file("/NOTES.DAT", (uint8_t*)notes, sizeof(notes));
}

static void RenderHeaderBg(Control* self, Form* f) {
    gui_form_draw_rect(f, self->x, self->y, self->w, self->h, 0x000080);
}

// Style plaskie, bez ramki 3D - inne niz przyciski Kalkulatora/WinVer/Ustawien z Faz 1-2 (te
// maja bezel highlight/shadow), wiec to trzeci, osobny styl - nie ma sensu wyciagac wspolnego
// helpera do widgets.c, skoro nawet ta apka ma dwa lekko rozne uzycia tego samego pomyslu
// (RenderNavButton z ustalonym przesunieciem tekstu, RenderDayCell z innym) i tak jest krotki.
static void RenderNavButton(Control* self, Form* f) {
    gui_form_draw_rect(f, self->x, self->y, self->w, self->h, 0xC0C0C0);
    gui_form_draw_string(f, self->text, self->x + 6, self->y + 6, 0x000000, 0xC0C0C0);
}

static void RenderHeaderLabel(Control* self, Form* f) {
    gui_form_draw_string(f, self->text, self->x, self->y, 0xFFFFFF, 0x000080);
}

static void RenderDayCell(Control* self, Form* f) {
    int day_num = (int)(intptr_t)self->user_data;
    uint32_t bg = (day_num == selected_day) ? 0x808080 : 0xC0C0C0;
    gui_form_draw_rect(f, self->x, self->y, self->w, self->h, bg);

    char d_str[16];
    IntToString(day_num, d_str);
    gui_form_draw_string(f, d_str, self->x + 2, self->y + 4, 0x000000, bg);
}

static void OnDayClick(Control* self) {
    selected_day = (int)(intptr_t)self->user_data;
    cursor_pos = 0;
    while (notes[selected_day - 1][cursor_pos]) cursor_pos++;
}

// Planer notatek pod siatka dni - reczne rysowanie znak-po-znaku z kursorem, dokladnie jak w
// oryginale. To jest juz w istocie "pole tekstowe z kursorem" ktore PLAN_ui_library.md nazywa
// jako potrzebne dla Notatnika - ale to osobna, do przegadania decyzja o WSPOLNEJ kontrolce
// biblioteki; tutaj zostaje jednorazowym, lokalnym ControlRenderFn (jak wyswietlacz Kalkulatora
// czy baner WinVer), nie nowym typem w widgets.c. Klawiatura dochodzi do apki przez nowe
// Form::on_key (patrz OnKeyPress nizej) - addytywny dodatek do gui_form_run, analogiczny do
// on_tick z Fazy 2, zero zmian w already-migrated Kalkulatorze/WinVer/Ustawieniach.
static void RenderNotesArea(Control* self, Form* f) {
    gui_form_draw_rect(f, self->x, self->y, self->w, self->h, 0xFFFFCC);

    if (selected_day >= 1 && selected_day <= 31) {
        char* note = notes[selected_day - 1];
        int cx = self->x + 5;
        int cy = self->y + 10;
        for (int i = 0; note[i] != '\0'; i++) {
            if (note[i] == '\n') {
                cx = self->x + 5;
                cy += 16;
            } else {
                char str[2] = {note[i], 0};
                gui_form_draw_string(f, str, cx, cy, 0x000000, 0xFFFFCC);
                cx += 8;
            }
        }
        gui_form_draw_rect(f, cx, cy, 8, 16, 0x000000);
    }
}

static void UpdateHeaderLabels(void) {
    IntToString(current_month, month_label->text);
    IntToString(current_year, year_label->text);
}

static void RebuildDayGrid(void) {
    form.control_count = GRID_START_INDEX;

    int start_day = GetDayOfWeek(1, current_month, current_year);
    int days = GetDaysInMonth(current_month, current_year);

    int cell_w = 30;
    int cell_h = 20;
    int grid_x = (win_w - 7 * cell_w) / 2;
    int grid_y = 50;

    for (int i = 0; i < days; i++) {
        int col = (start_day + i) % 7;
        int row = (start_day + i) / 7;
        int px = grid_x + col * cell_w;
        int py = grid_y + row * cell_h;

        gui_form_add_control(&form, px, py, cell_w - 2, cell_h - 2, "", RenderDayCell, OnDayClick,
                              (void*)(intptr_t)(i + 1));
    }
}

static void OnPrevMonth(Control* self) {
    (void)self;
    current_month--;
    if (current_month < 1) { current_month = 12; current_year--; }
    selected_day = 1;
    cursor_pos = 0;
    while (notes[selected_day - 1][cursor_pos]) cursor_pos++;
    UpdateHeaderLabels();
    RebuildDayGrid();
}

static void OnNextMonth(Control* self) {
    (void)self;
    current_month++;
    if (current_month > 12) { current_month = 1; current_year++; }
    selected_day = 1;
    cursor_pos = 0;
    while (notes[selected_day - 1][cursor_pos]) cursor_pos++;
    UpdateHeaderLabels();
    RebuildDayGrid();
}

static void OnKeyPress(Form* f, char c) {
    (void)f;
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

void _start() {
    win_id = sys_create_window("Kalendarz", win_w, win_h, 250, 80, &fb);
    if (win_id < 0 || !fb) sys_exit();

    struct DateTime dt;
    if (sys_get_time(&dt)) {
        current_year = 2000 + dt.year;
        current_month = dt.month;
        selected_day = dt.day;
    }

    for (int i = 0; i < 31; i++) {
        for (int j = 0; j < 128; j++) notes[i][j] = '\0';
    }
    LoadNotes();
    cursor_pos = 0;
    while (notes[selected_day - 1][cursor_pos]) cursor_pos++;

    gui_form_init(&form, win_id, fb, win_w, win_h, 0xFFFFFF);
    form.on_key = OnKeyPress;

    gui_form_add_control(&form, 0, 0, win_w, 30, NULL, RenderHeaderBg, NULL, NULL);
    gui_form_add_control(&form, 5, 5, 20, 20, "<", RenderNavButton, OnPrevMonth, NULL);
    gui_form_add_control(&form, win_w - 25, 5, 20, 20, ">", RenderNavButton, OnNextMonth, NULL);
    gui_form_add_control(&form, 35, 11, 0, 0, "Miesiac:", RenderHeaderLabel, NULL, NULL);
    month_label = gui_form_add_control(&form, 105, 11, 0, 0, "", RenderHeaderLabel, NULL, NULL);
    gui_form_add_control(&form, 135, 11, 0, 0, "Rok:", RenderHeaderLabel, NULL, NULL);
    year_label = gui_form_add_control(&form, 175, 11, 0, 0, "", RenderHeaderLabel, NULL, NULL);

    int planner_y = 50 + 6 * 20 + 10;
    int planner_h = win_h - planner_y - 5;
    gui_form_add_control(&form, 5, planner_y, win_w - 10, planner_h, NULL, RenderNotesArea, NULL, NULL);

    UpdateHeaderLabels();
    RebuildDayGrid();

    gui_form_paint(&form);
    sys_update_window(win_id);

    gui_form_run(&form);
}
