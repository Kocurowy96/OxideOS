#include <gui.h>
#include <widgets.h>
#include <stddef.h>

static void itoa(uint64_t val, char* buf) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    int pos = 0;
    char rev[32];
    while (val > 0) {
        rev[pos++] = (val % 10) + '0';
        val /= 10;
    }
    int dpos = 0;
    while (pos > 0) {
        buf[dpos++] = rev[--pos];
    }
    buf[dpos] = '\0';
}

static void strcpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

static void strcat(char* dest, const char* src) {
    while (*dest) dest++;
    while (*src) *dest++ = *src++;
    *dest = '\0';
}

Form form;
Control* ram_label;
uint64_t g_total_mem = 0;
uint64_t g_last_free_mem = 0;

static void UpdateRamText(void) {
    char num_buf[32];
    strcpy(ram_label->text, "Pamiec fizyczna RAM: ");
    itoa(g_total_mem / (1024 * 1024), num_buf);
    strcat(ram_label->text, num_buf);
    strcat(ram_label->text, " MB (Wolne: ");
    itoa(g_last_free_mem / (1024 * 1024), num_buf);
    strcat(ram_label->text, num_buf);
    strcat(ram_label->text, " MB)");
}

// winver.bmp to baner rysowany bezposrednio syscallem (sys_draw_bmp), nie przez fb - inaczej
// niz reszta kontrolek. Wlasny render zamiast wbudowanej etykiety, bo trzeba wywolac ten syscall
// przy kazdym gui_form_paint (pelne przemalowanie czysci cale okno, wiec baner trzeba odrysowac
// razem z reszta, nie tylko raz przy starcie jak w oryginale).
static void RenderBanner(Control* self, Form* f) {
    sys_draw_bmp(f->win_id, "/winver.bmp", self->x, self->y);
}

static void OnOkClick(Control* self) {
    (void)self;
    sys_exit();
}

static void OnTick(Form* f) {
    uint64_t free_mem = 0;
    sys_get_mem_info(&g_total_mem, &free_mem);
    if (free_mem != g_last_free_mem) {
        g_last_free_mem = free_mem;
        UpdateRamText();
        gui_form_paint(f);
        sys_update_window(f->win_id);
    }
}

void _start() {
    uint32_t* fb = 0;
    int win_w = 500;
    int win_h = 360;
    int win_id = sys_create_window("O Systemie (WINVER)", win_w, win_h, 400, 200, &fb);

    if (win_id < 0 || !fb) {
        sys_print("Failed to create Winver window\n");
        sys_exit();
    }

    gui_form_init(&form, win_id, fb, win_w, win_h, 0xC0C0C0);
    form.on_tick = OnTick;

    // Szerokosc baneru to 400px, okno ma 500px, (500 - 400) / 2 = 50.
    gui_form_add_control(&form, 50, 20, 400, 100, NULL, RenderBanner, NULL, NULL);

    gui_form_add_label(&form, 50, 180, "System operacyjny OxideOS");
    gui_form_add_label(&form, 50, 200, "Wersja jadra 1.0.0");

    sys_get_mem_info(&g_total_mem, &g_last_free_mem);
    ram_label = gui_form_add_label(&form, 50, 230, "");
    UpdateRamText();

    int btn_w = 80;
    int btn_h = 24;
    int btn_x = (win_w - btn_w) / 2;
    int btn_y = win_h - 40;
    gui_form_add_thin_button(&form, btn_x, btn_y, btn_w, btn_h, "OK", OnOkClick, NULL);

    gui_form_paint(&form);
    sys_update_window(win_id);

    gui_form_run(&form);
}
