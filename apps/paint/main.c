#include <gui.h>
#include <widgets.h>
#include <stddef.h>
#include <stdint.h>

#define CELL_SIZE 6

Form form;
int win_id;
uint32_t* fb;
int win_w = 260;
int win_h = 300;

uint8_t canvas[32][32];
uint32_t current_color = 1;  // domyslnie "GUMKA" (biale tlo), tak jak w oryginale
const uint32_t paint_palette[8] = {
    0x000000, 0xFFFFFF, 0xFF0000, 0x00FF00,
    0x0000FF, 0xFFFF00, 0x00FFFF, 0xFF00FF
};

static void RenderGrid(Control* self, Form* f) {
    gui_form_draw_rect(f, self->x - 2, self->y - 2, self->w + 4, self->h + 4, 0x000000);
    gui_form_draw_rect(f, self->x - 1, self->y - 1, self->w + 2, self->h + 2, 0xFFFFFF);

    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            gui_form_draw_rect(f, self->x + x * CELL_SIZE, self->y + y * CELL_SIZE,
                                CELL_SIZE, CELL_SIZE, paint_palette[canvas[x][y]]);
        }
    }
}

// Siatka to jedna kontrolka (zamiast 1024 osobnych Control na komorke, dawno ponad
// FORM_MAX_CONTROLS) - obsluguje kliki wewnatrz siebie przez form->last_click_x/y
// (Faza 5, patrz widgets.h), odejmujac wlasne self->x/self->y zeby policzyc lokalna komorke.
static void OnGridClick(Control* self) {
    int local_x = form.last_click_x - self->x;
    int local_y = form.last_click_y - self->y;
    if (local_x < 0 || local_x >= self->w || local_y < 0 || local_y >= self->h) return;

    int cx = local_x / CELL_SIZE;
    int cy = local_y / CELL_SIZE;
    if (cx >= 0 && cx < 32 && cy >= 0 && cy < 32) {
        canvas[cx][cy] = current_color;
    }
}

static void RenderPalettePanel(Control* self, Form* f) {
    gui_form_draw_rect(f, self->x, self->y, self->w, self->h, 0xC0C0C0);
    gui_form_draw_rect(f, self->x, self->y, self->w, 2, 0xFFFFFF);
    gui_form_draw_rect(f, self->x, self->y, 2, self->h, 0xFFFFFF);
    gui_form_draw_rect(f, self->x + self->w - 2, self->y, 2, self->h, 0x000000);
    gui_form_draw_rect(f, self->x, self->y + self->h - 2, self->w, 2, 0x000000);
}

static void RenderSwatch(Control* self, Form* f) {
    int idx = (int)(intptr_t)self->user_data;
    gui_form_draw_rect(f, self->x, self->y, self->w, self->h, paint_palette[idx]);

    if (current_color == (uint32_t)idx) {
        gui_form_draw_rect(f, self->x - 2, self->y - 2, self->w + 4, 2, 0x000000);
        gui_form_draw_rect(f, self->x - 2, self->y - 2, 2, self->h + 4, 0x000000);
        gui_form_draw_rect(f, self->x + self->w + 2, self->y - 2, 2, self->h + 4, 0x000000);
        gui_form_draw_rect(f, self->x - 2, self->y + self->h + 2, self->w + 4, 2, 0x000000);
    }
}

static void OnSwatchClick(Control* self) {
    current_color = (uint32_t)(intptr_t)self->user_data;
}

// Styl identyczny z widgets.c::ButtonRender (uzywanym przez CZYSC/ZAPISZ nizej), z dopisana
// ramka zaznaczenia gdy GUMKA jest aktywnym "narzedziem" (current_color == 1) - stad wlasny
// ControlRenderFn zamiast gui_form_add_button.
static void RenderEraserButton(Control* self, Form* f) {
    gui_form_draw_rect(f, self->x, self->y, self->w, self->h, 0x808080);
    gui_form_draw_rect(f, self->x, self->y, self->w, 2, 0xFFFFFF);
    gui_form_draw_rect(f, self->x, self->y, 2, self->h, 0xFFFFFF);
    gui_form_draw_rect(f, self->x + self->w - 2, self->y, 2, self->h, 0x000000);
    gui_form_draw_rect(f, self->x, self->y + self->h - 2, self->w, 2, 0x000000);

    int len = 0;
    while (self->text[len]) len++;
    int tx = self->x + (self->w - len * 8) / 2;
    int ty = self->y + (self->h - 8) / 2;
    gui_form_draw_string(f, self->text, tx, ty, 0x000000, 0x808080);

    if (current_color == 1) {
        gui_form_draw_rect(f, self->x - 2, self->y - 2, self->w + 4, 2, 0x000000);
        gui_form_draw_rect(f, self->x - 2, self->y - 2, 2, self->h + 4, 0x000000);
        gui_form_draw_rect(f, self->x + self->w, self->y - 2, 2, self->h + 4, 0x000000);
        gui_form_draw_rect(f, self->x - 2, self->y + self->h, self->w + 4, 2, 0x000000);
    }
}

static void OnEraserClick(Control* self) {
    (void)self;
    current_color = 1;
}

static void OnClearClick(Control* self) {
    (void)self;
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            canvas[x][y] = 1;
        }
    }
}

static void OnSaveClick(Control* self) {
    (void)self;
    sys_write_file("/DOCS/PAINT.RAW", (const uint8_t*)canvas, sizeof(canvas));
}

void _start() {
    win_id = sys_create_window("Paint", win_w, win_h, 200, 50, &fb);
    if (win_id < 0 || !fb) sys_exit();

    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            canvas[x][y] = 1;
        }
    }

    gui_form_init(&form, win_id, fb, win_w, win_h, 0x808080);

    int grid_x = (win_w - 32 * CELL_SIZE) / 2;
    int grid_y = 8;
    gui_form_add_control(&form, grid_x, grid_y, 32 * CELL_SIZE, 32 * CELL_SIZE, NULL,
                          RenderGrid, OnGridClick, NULL);

    int palette_y = win_h - 60;
    gui_form_add_control(&form, 4, palette_y, win_w - 8, 56, NULL, RenderPalettePanel, NULL, NULL);

    for (int i = 0; i < 8; i++) {
        int px = 12 + i * 28;
        int py = palette_y + 6;
        gui_form_add_control(&form, px, py, 20, 20, NULL, RenderSwatch, OnSwatchClick,
                              (void*)(intptr_t)i);
    }

    int btn_py = palette_y + 32;
    gui_form_add_control(&form, 12, btn_py, 60, 20, "GUMKA", RenderEraserButton, OnEraserClick, NULL);
    gui_form_add_button(&form, 80, btn_py, 70, 20, "CZYSC", OnClearClick, NULL);
    gui_form_add_button(&form, 160, btn_py, 70, 20, "ZAPISZ", OnSaveClick, NULL);

    gui_form_paint(&form);
    sys_update_window(win_id);

    gui_form_run(&form);
}
