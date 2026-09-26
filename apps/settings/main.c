#include <gui.h>
#include <widgets.h>
#include <stddef.h>
#include <stdint.h>

static void itoa(uint32_t val, char* buf) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    int pos = 0;
    char rev[16];
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

static void strcpy_(char* dest, const char* src) {
    while (*src) { *dest++ = *src++; }
    *dest = '\0';
}

static void strcat_(char* dest, const char* src) {
    while (*dest) dest++;
    while (*src) { *dest++ = *src++; }
    *dest = '\0';
}

// Ilosc kontrolek zawsze obecnych niezaleznie od zakladki: tlo sidebaru (1) + 5 pozycji zakladek.
// RebuildContent() obcina Form::control_count z powrotem do tej wartosci przy kazdej zmianie
// zakladki, zeby dodac na nowo tylko kontrolki wlasciwe nowej zakladce (sidebar zostaje
// nietkniety, dodany raz w _start()).
#define CONTENT_START_INDEX 6

Form form;
int g_state = 0; // 0 = Wyglad, 1 = Wyswietlacz, 2 = System, 3 = Dzwiek, 4 = Mysz
uint32_t g_volume = 80;
uint32_t g_mouse_speed = 100;
Control* volume_label = NULL;
Control* speed_label = NULL;

typedef struct {
    const char* thumb_path;
    const char* wallpaper_path;
    const char* fallback_text;
} WallpaperOption;

static const WallpaperOption wp_options[3] = {
    {"/PICS/wp1_thumb.bmp", "/wp1.bmp", "Brak wp1_thumb"},
    {"/PICS/wp2_thumb.bmp", "/wp2.bmp", "Brak wp2_thumb"},
    {"/PICS/wp3_thumb.bmp", "/wp3.bmp", "Brak wp3_thumb"},
};

static void RenderSidebarBg(Control* self, Form* f) {
    gui_form_draw_rect(f, self->x, self->y, self->w, self->h, 0x2C3E50);
}

static void RenderThumbnail(Control* self, Form* f) {
    const WallpaperOption* opt = (const WallpaperOption*)self->user_data;
    if (!sys_draw_bmp(f->win_id, opt->thumb_path, self->x, self->y)) {
        gui_form_draw_rect(f, self->x, self->y, self->w, self->h, 0xBDC3C7);
        gui_form_draw_string(f, opt->fallback_text, self->x + 20, self->y + 40, 0x000000, 0xBDC3C7);
    }
}

static void OnThumbnailClick(Control* self) {
    const WallpaperOption* opt = (const WallpaperOption*)self->user_data;
    sys_write_file("/DOCS/CONFIG.DAT", (const uint8_t*)opt->wallpaper_path, 8);
    sys_reload_wallpaper();
}

// Styl 1:1 jak dawny draw_win_button (WinVer ma bardzo podobny recznie rysowany przycisk OK,
// ale z innym, na sztywno wyliczonym przesunieciem tekstu - tu zostaje osobna, lokalna kopia
// zamiast wspolnej funkcji w widgets.c, zgodnie z konwencja projektu: male powtorzenie >
// przedwczesna abstrakcja, patrz HOW_WE_WORK.md).
static void RenderThinButton(Control* self, Form* f) {
    gui_form_draw_rect(f, self->x, self->y, self->w, self->h, 0xC0C0C0);
    gui_form_draw_rect(f, self->x, self->y, self->w, 1, 0xFFFFFF);
    gui_form_draw_rect(f, self->x, self->y, 1, self->h, 0xFFFFFF);
    gui_form_draw_rect(f, self->x + self->w - 1, self->y, 1, self->h, 0x000000);
    gui_form_draw_rect(f, self->x, self->y + self->h - 1, self->w, 1, 0x000000);

    int len = 0;
    while (self->text[len]) len++;
    int tx = self->x + (self->w - len * 8) / 2;
    int ty = self->y + (self->h - 8) / 2;
    gui_form_draw_string(f, self->text, tx, ty, 0x000000, 0xC0C0C0);
}

static void RefreshVolumeLabel(void) {
    char num[16];
    strcpy_(volume_label->text, "Glosnosc: ");
    itoa(g_volume, num);
    strcat_(volume_label->text, num);
    strcat_(volume_label->text, "%");
}

static void OnVolumeDown(Control* self) {
    (void)self;
    if (g_volume >= 10) {
        g_volume -= 10;
        sys_set_volume(g_volume);
        RefreshVolumeLabel();
    }
}

static void OnVolumeUp(Control* self) {
    (void)self;
    if (g_volume <= 90) {
        g_volume += 10;
        sys_set_volume(g_volume);
        RefreshVolumeLabel();
    }
}

static void OnTestSound(Control* self) {
    (void)self;
    sys_play_wav("/notify.wav");
}

static void RefreshSpeedLabel(void) {
    char num[16];
    strcpy_(speed_label->text, "Czulosc: ");
    itoa(g_mouse_speed, num);
    strcat_(speed_label->text, num);
    strcat_(speed_label->text, "%");
}

static void OnMouseSpeedDown(Control* self) {
    (void)self;
    if (g_mouse_speed >= 50) {
        g_mouse_speed -= 25;
        sys_set_mouse_speed(g_mouse_speed);
        RefreshSpeedLabel();
    }
}

static void OnMouseSpeedUp(Control* self) {
    (void)self;
    if (g_mouse_speed <= 275) {
        g_mouse_speed += 25;
        sys_set_mouse_speed(g_mouse_speed);
        RefreshSpeedLabel();
    }
}

static void RebuildContent(int state) {
    form.control_count = CONTENT_START_INDEX;
    volume_label = NULL;
    speed_label = NULL;

    if (state == 0) {
        gui_form_add_label(&form, 160, 20, "Ustawienia - Wyglad");
        gui_form_add_label(&form, 160, 60, "Wybierz tapete:");
        gui_form_add_control(&form, 160, 90, 160, 90, "", RenderThumbnail, OnThumbnailClick, (void*)&wp_options[0]);
        gui_form_add_control(&form, 340, 90, 160, 90, "", RenderThumbnail, OnThumbnailClick, (void*)&wp_options[1]);
        gui_form_add_control(&form, 160, 200, 160, 90, "", RenderThumbnail, OnThumbnailClick, (void*)&wp_options[2]);
    } else if (state == 1) {
        gui_form_add_label(&form, 160, 20, "Ustawienia - Wyswietlacz");

        uint32_t width = 0, height = 0, bpp = 0;
        sys_get_display_info(&width, &height, &bpp);
        char line[64];
        char num[16];

        strcpy_(line, "Rozdzielczosc: ");
        itoa(width, num); strcat_(line, num);
        strcat_(line, " x ");
        itoa(height, num); strcat_(line, num);
        gui_form_add_label(&form, 160, 60, line);

        strcpy_(line, "Glebia koloru: ");
        itoa(bpp, num); strcat_(line, num);
        strcat_(line, " bit");
        gui_form_add_label(&form, 160, 80, line);

        gui_form_add_label(&form, 160, 100, "Zrodlo: Limine Framebuffer (GOP/VESA)");
        gui_form_add_label(&form, 160, 120, "Zmiana rozdzielczosci: niedostepna");
    } else if (state == 2) {
        gui_form_add_label(&form, 160, 20, "Ustawienia - System");
        gui_form_add_label(&form, 160, 60, "Wersja: OxideOS v1.1.0");
        gui_form_add_label(&form, 160, 80, "Jadro: 64-bit Long Mode");
        gui_form_add_label(&form, 160, 100, "Menedzer: Wlasny Compositor");

        uint64_t total_mem = 0, free_mem = 0;
        sys_get_mem_info(&total_mem, &free_mem);

        char line[64];
        char num[16];
        strcpy_(line, "Pamiec RAM: ");
        itoa((uint32_t)(total_mem / (1024 * 1024)), num); strcat_(line, num);
        strcat_(line, " MB (Wolne: ");
        itoa((uint32_t)(free_mem / (1024 * 1024)), num); strcat_(line, num);
        strcat_(line, " MB)");
        gui_form_add_label(&form, 160, 120, line);
    } else if (state == 3) {
        gui_form_add_label(&form, 160, 20, "Ustawienia - Dzwiek");

        char line[32];
        char num[16];
        strcpy_(line, "Glosnosc: ");
        itoa(g_volume, num); strcat_(line, num);
        strcat_(line, "%");
        volume_label = gui_form_add_label(&form, 160, 60, line);

        gui_form_add_control(&form, 160, 85, 40, 30, "-", RenderThinButton, OnVolumeDown, NULL);
        gui_form_add_control(&form, 210, 85, 40, 30, "+", RenderThinButton, OnVolumeUp, NULL);
        gui_form_add_control(&form, 160, 135, 160, 30, "Testuj dzwiek", RenderThinButton, OnTestSound, NULL);
    } else if (state == 4) {
        gui_form_add_label(&form, 160, 20, "Ustawienia - Mysz");

        char line[32];
        char num[16];
        strcpy_(line, "Czulosc: ");
        itoa(g_mouse_speed, num); strcat_(line, num);
        strcat_(line, "%");
        speed_label = gui_form_add_label(&form, 160, 60, line);

        gui_form_add_control(&form, 160, 85, 40, 30, "-", RenderThinButton, OnMouseSpeedDown, NULL);
        gui_form_add_control(&form, 210, 85, 40, 30, "+", RenderThinButton, OnMouseSpeedUp, NULL);
        gui_form_add_label(&form, 160, 165, "(dziala na PS/2 w trybie relatywnym)");
    }
}

static void OnTabClick(Control* self) {
    int tab_id = (int)(intptr_t)self->user_data;
    if (tab_id == g_state) return;
    g_state = tab_id;
    form.active_tab = tab_id;
    RebuildContent(tab_id);
}

void _start() {
    uint32_t* fb = 0;
    int win_w = 540;
    int win_h = 360;
    int win_id = sys_create_window("Panel Sterowania", win_w, win_h, 100, 100, &fb);

    if (win_id < 0 || !fb) {
        sys_print("Failed to create Settings window\n");
        sys_exit();
    }

    sys_get_volume(&g_volume);
    sys_get_mouse_speed(&g_mouse_speed);

    gui_form_init(&form, win_id, fb, win_w, win_h, 0xECF0F1);
    form.active_tab = 0;

    gui_form_add_control(&form, 0, 0, 140, win_h, NULL, RenderSidebarBg, NULL, NULL);
    gui_form_add_tab(&form, 0, 40, 140, 40, "Wyglad", 0, OnTabClick);
    gui_form_add_tab(&form, 0, 80, 140, 40, "Wyswietlacz", 1, OnTabClick);
    gui_form_add_tab(&form, 0, 120, 140, 40, "System", 2, OnTabClick);
    gui_form_add_tab(&form, 0, 160, 140, 40, "Dzwiek", 3, OnTabClick);
    gui_form_add_tab(&form, 0, 200, 140, 40, "Mysz", 4, OnTabClick);

    RebuildContent(g_state);

    gui_form_paint(&form);
    sys_update_window(win_id);

    gui_form_run(&form);
}
