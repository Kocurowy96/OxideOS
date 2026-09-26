#include <gui.h>
#include <widgets.h>
#include <stddef.h>
#include <stdint.h>

int win_id;
uint32_t* fb;
int win_w = 400;
int win_h = 300;

Form form;
struct TaskInfo tasks[16];
int task_count = 0;
int selected_task = -1;
int tick_counter = 0;

// Kontrolki zawsze obecne: etykieta "Zadania:" (0), przycisk "Zakoncz" (1), tlo/ramka listy
// (2). RebuildTaskList() obcina Form::control_count z powrotem do tej wartosci przy kazdym
// odswiezeniu listy zadan i dodaje na nowo wiersze + na koncu "wylapywacz" pustego miejsca w
// liscie (patrz OnListBgClick nizej) - ten sam wzorzec co RebuildContent()/RebuildDayGrid()
// z Ustawien/Kalendarza (Fazy 2-3).
#define LIST_ROWS_START 3

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
    for (int i = len - 1; i >= 0; i--) {
        str[i] = (val % 10) + '0';
        val /= 10;
    }
}

static void RenderListBg(Control* self, Form* f) {
    gui_form_draw_rect(f, self->x, self->y, self->w, self->h, 0xFFFFFF);
    gui_form_draw_rect(f, self->x - 1, self->y - 1, self->w + 2, 1, 0x000000);
    gui_form_draw_rect(f, self->x - 1, self->y - 1, 1, self->h + 2, 0x000000);
}

static void RenderTaskRow(Control* self, Form* f) {
    int idx = (int)(intptr_t)self->user_data;
    uint32_t bg = (idx == selected_task) ? 0x000080 : 0xFFFFFF;
    uint32_t fg = (idx == selected_task) ? 0xFFFFFF : 0x000000;
    gui_form_draw_rect(f, self->x, self->y, self->w, self->h, bg);

    char pid_str[16];
    IntToString(tasks[idx].id, pid_str);
    gui_form_draw_string(f, pid_str, self->x + 2, self->y + 4, fg, bg);
    gui_form_draw_string(f, tasks[idx].name, self->x + 50, self->y + 4, fg, bg);
}

static void OnTaskRowClick(Control* self) {
    selected_task = (int)(intptr_t)self->user_data;
}

// Klik gdziekolwiek w obrebie listy, ale NIE trafiajacy w zaden wiersz (puste miejsce ponizej
// ostatniego zadania) - odznacza wybor, dokladnie jak w oryginale. Dziala poprawnie dzieki
// kolejnosci dodawania: wiersze sa wczesniej w tablicy Form::controls niz ten kontroler, a
// gui_form_run zatrzymuje hit-test na PIERWSZYM trafieniu (i tak wiersz zawsze wygrywa nad
// tym wiekszym, tlem pokrywajacym obszarem, gdy klik faktycznie trafi w konkretny wiersz).
static void OnListBgClick(Control* self) {
    (void)self;
    selected_task = -1;
}

static void RebuildTaskList(void) {
    form.control_count = LIST_ROWS_START;

    for (int i = 0; i < task_count; i++) {
        int y = 32 + i * 16;
        gui_form_add_control(&form, 10, y, 380, 16, "", RenderTaskRow, OnTaskRowClick, (void*)(intptr_t)i);
    }

    gui_form_add_control(&form, 10, 30, 380, 200, NULL, NULL, OnListBgClick, NULL);
}

static void OnZakonczClick(Control* self) {
    (void)self;
    if (selected_task >= 0 && selected_task < task_count) {
        sys_kill_task(tasks[selected_task].id);
        selected_task = -1;
        task_count = sys_get_tasks(tasks, 16);
        RebuildTaskList();
    }
}

// Odswiezanie listy niezalezne od klikniec (nowe/zakonczone zadania z zewnatrz) - ten sam
// wzorzec co Form::on_tick w WinVer (Faza 2), z tym samym throttlingiem co oryginal (co ~50
// przebiegow petli, nie za kazdym razem).
static void OnTick(Form* f) {
    (void)f;
    tick_counter++;
    if (tick_counter > 50) {
        tick_counter = 0;
        int new_count = sys_get_tasks(tasks, 16);
        if (new_count != task_count) {
            task_count = new_count;
            if (selected_task >= task_count) selected_task = -1;
            RebuildTaskList();
        }
    }
}

void _start() {
    win_id = sys_create_window("Menedzer Zadan", win_w, win_h, 300, 200, &fb);
    if (win_id < 0 || !fb) sys_exit();

    task_count = sys_get_tasks(tasks, 16);

    gui_form_init(&form, win_id, fb, win_w, win_h, 0xC0C0C0);
    form.on_tick = OnTick;

    gui_form_add_label(&form, 10, 10, "Zadania:");
    gui_form_add_thin_button(&form, 270, 250, 120, 30, "Zakoncz", OnZakonczClick, NULL);
    gui_form_add_control(&form, 10, 30, 380, 200, NULL, RenderListBg, NULL, NULL);

    RebuildTaskList();

    gui_form_paint(&form);
    sys_update_window(win_id);

    gui_form_run(&form);
}
