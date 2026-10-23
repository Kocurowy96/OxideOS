#include "compositor.h"
#include "fb.h"
#include "bmp.h"
#include "embedded_assets.h"
#include "../cpu/syscall.h"
#include "../drivers/ps2_mouse.h"
#include "../drivers/ac97.h"
#include "../drivers/rtc.h"
#include "application.h"
#include "apps.h"
#include "../mem/pmm.h"
#include "../mem/vmm.h"
#include "../fs/vfs.h"
#include "../proc/sched.h"
#include "../limine.h"

extern void ExecAppTask(void*);
#include <stddef.h>

inline void* operator new(size_t, void* p) { return p; }
inline void* operator new[](size_t, void* p) { return p; }

#include "../fs/vfs.h"
#include "../serial.h"

extern volatile struct limine_hhdm_request hhdm_request;

static void* bg_bmp = nullptr;
void* Compositor::icon_bmp = nullptr;
void* Compositor::cursor_bmp = nullptr;
static void* icon_settings = nullptr; // For later if added
static void* icon_speaker = nullptr;

Window* Compositor::windows[MAX_WINDOWS];
Window* Compositor::taskbar_windows[MAX_WINDOWS];
int Compositor::window_count = 0;
volatile bool Compositor::is_rendering = false;

bool Compositor::AddWindow(Window* win) {
    if (window_count >= MAX_WINDOWS) return false;
    windows[window_count] = win;
    taskbar_windows[window_count] = win;
    window_count++;
    return true;
}

void Compositor::RemoveWindow(Window* win) {
    int index = -1;
    for (int i = 0; i < window_count; i++) {
        if (windows[i] == win) {
            index = i;
            break;
        }
    }
    if (index == -1) return;
    for (int i = index; i < window_count - 1; i++) {
        windows[i] = windows[i + 1];
    }
    window_count--;
}

void Compositor::RemoveWindowsByTaskId(uint64_t task_id) {
    for (int i = 0; i < window_count; i++) {
        if (windows[i]->owner_task_id == task_id) {
            windows[i]->pending_remove = true;
        }
    }
}

Window* Compositor::GetWindowById(int id) {
    for (int i = 0; i < window_count; i++) {
        if (windows[i]->id == id) return windows[i];
    }
    return nullptr;
}

void Compositor::BringToFront(Window* win) {
    int index = -1;
    for (int i = 0; i < window_count; i++) {
        if (windows[i] == win) {
            index = i;
            break;
        }
    }
    if (index == -1 || index == window_count - 1) return;
    for (int i = index; i < window_count - 1; i++) {
        windows[i] = windows[i + 1];
    }
    windows[window_count - 1] = win;
}

static int frame_count = 0;
static uint32_t last_ticks = 0;
static int fps = 0;
static char fps_str[16];

static bool prev_mouse_left = false;
static bool start_menu_open = false;

// Menu Start: pozycje wczytywane dynamicznie z /usr/bin (VFS::ListDirectory),
// rekursywnie - podkatalogi otwierają się jako kaskadowe submenu (Win95-style).
#define MAX_START_MENU_ITEMS 32
#define MAX_MENU_DEPTH 3

struct StartMenuLevel {
    DirEntry entries[MAX_START_MENU_ITEMS];
    char paths[MAX_START_MENU_ITEMS][160];
    int count;
    int x, y;
    int open_child_index; // indeks pozycji z aktualnie otwartym submenu (level+1), -1 jeśli brak
};
static StartMenuLevel menu_levels[MAX_MENU_DEPTH];
static int menu_open_depth = 0; // ile poziomów (0..) jest obecnie widocznych; 0 = menu zamknięte

static bool str_ends_with(const char* str, const char* suffix) {
    int str_len = 0; while (str[str_len]) str_len++;
    int suf_len = 0; while (suffix[suf_len]) suf_len++;
    if (suf_len > str_len) return false;
    for (int i = 0; i < suf_len; i++) {
        if (str[str_len - suf_len + i] != suffix[i]) return false;
    }
    return true;
}

// Odświeża listę pozycji danego poziomu menu z zawartości dir_path (katalogu)
static void RefreshMenuLevel(int level, const char* dir_path) {
    StartMenuLevel* lvl = &menu_levels[level];
    DirEntry raw_entries[MAX_START_MENU_ITEMS];
    int raw_count = VFS::ListDirectory(dir_path, raw_entries, MAX_START_MENU_ITEMS);

    lvl->count = 0;
    lvl->open_child_index = -1;
    for (int i = 0; i < raw_count && lvl->count < MAX_START_MENU_ITEMS; i++) {
        DirEntry* src = &raw_entries[i];
        // Pokazujemy uruchamialne pliki .ELF oraz podkatalogi (jako kaskadowe submenu)
        bool is_dir = (src->attributes & FS_ATTR_DIRECTORY) != 0;
        if (!is_dir && !str_ends_with(src->name, ".ELF")) continue;

        DirEntry* dst = &lvl->entries[lvl->count];
        *dst = *src;

        char* path = lvl->paths[lvl->count];
        int p = 0;
        while (dir_path[p]) { path[p] = dir_path[p]; p++; }
        if (p == 0 || path[p - 1] != '/') path[p++] = '/';
        int n = 0;
        while (src->name[n] && p < 159) { path[p++] = src->name[n++]; }
        path[p] = '\0';

        lvl->count++;
    }
}

// Odświeża Menu Start (poziom 0, /usr/bin) i zamyka wszystkie otwarte submenu
static void RefreshStartMenu() {
    RefreshMenuLevel(0, "/usr/bin");
    menu_open_depth = 1;
}

void itoa(int n, char* buffer) {
    int i = 0;
    if (n == 0) {
        buffer[i++] = '0';
        buffer[i] = '\0';
        return;
    }
    while (n > 0) {
        buffer[i++] = (n % 10) + '0';
        n /= 10;
    }
    buffer[i] = '\0';
    // odwróć string
    for (int j = 0; j < i / 2; j++) {
        char temp = buffer[j];
        buffer[j] = buffer[i - j - 1];
        buffer[i - j - 1] = temp;
    }
}

void Compositor::InitWallpaper() {
    uint8_t* buffer = nullptr;
    uint32_t size = 0;
    
    // Próbujemy wczytać z ustawień
    if (VFS::ReadFile("/DOCS/CONFIG.DAT", &buffer, &size)) {
        if (size >= 12 && buffer[0] == 'W' && buffer[1] == 'P' && buffer[2] == '=') {
            char wp_path[32];
            int i = 0;
            for(i=0; i<31 && i < size-3; i++) {
                if(buffer[i+3] == '\n' || buffer[i+3] == '\r') break;
                wp_path[i] = buffer[i+3];
            }
            wp_path[i] = '\0';
            
            uint8_t* wp_buf = nullptr;
            uint32_t wp_size = 0;
            if (VFS::ReadFile(wp_path, &wp_buf, &wp_size)) {
                bg_bmp = wp_buf;
                SerialPort::WriteString("Compositor: Loaded configured wallpaper!\n");
            }
        }
        delete[] buffer;
    }
    
    // Fallback: szukamy bg.bmp (jak kiedyś)
    if (!bg_bmp) {
        if (VFS::ReadFile("/bg.bmp", &buffer, &size)) {
            bg_bmp = buffer;
            SerialPort::WriteString("Compositor: Loaded default background (bg.bmp).\n");
        }
    }
}

void Compositor::Init() {
    InitWallpaper();

    // Cursor and generic window icon are baked into the kernel binary (see
    // kernel/gui/embedded_assets.h) instead of loaded from ext2 at runtime,
    // so the GUI always has them even if disk.img is missing or fails to mount.
    icon_bmp = EmbeddedAssets::icon_bmp;
    cursor_bmp = EmbeddedAssets::cursor_normal_bmp;

    uint8_t* speaker_buf = nullptr;
    uint32_t speaker_size = 0;
    if (VFS::ReadFile("/icon_speaker.bmp", &speaker_buf, &speaker_size)) {
        icon_speaker = speaker_buf;
    }
}
void Compositor::HandleKeyPress(char c) {
    if (window_count > 0) {
        Window* top_win = nullptr;
        for (int i = window_count - 1; i >= 0; i--) {
            if (!windows[i]->is_minimized) {
                top_win = windows[i];
                break;
            }
        }
        
        if (top_win && top_win->app) {
            top_win->app->OnKeyPress(c);
        } else if (top_win && top_win->fb_buffer) {
            Window::Event ev;
            ev.type = 2; // KeyPress
            ev.key = c;
            ev.x = 0; ev.y = 0;
            top_win->PushEvent(ev);
        }
    }
}

bool Compositor::prev_mouse_left = false;
bool Compositor::start_menu_open = false;
int Compositor::pending_click_frames = 0;

static void DrawButton(int x, int y, int w, int h, const char* text, bool pressed) {
    Framebuffer::DrawRect(x, y, w, h, 0xC0C0C0);
    if (pressed) {
        Framebuffer::DrawRect(x, y, w, 1, 0x000000); // inner shadow top
        Framebuffer::DrawRect(x, y, 1, h, 0x000000); // inner shadow left
    } else {
        Framebuffer::DrawRect(x, y, w, 1, 0xFFFFFF); // highlight top
        Framebuffer::DrawRect(x, y, 1, h, 0xFFFFFF); // highlight left
        Framebuffer::DrawRect(x + w - 1, y, 1, h, 0x000000); // shadow right
        Framebuffer::DrawRect(x, y + h - 1, w, 1, 0x000000); // shadow bottom
    }
    
    int text_len = 0;
    while(text[text_len]) text_len++;
    int text_w = text_len * 8;
    int text_x = x + (w - text_w) / 2;
    int text_y = y + (h - 16) / 2;
    
    if (pressed) {
        text_x += 1;
        text_y += 1;
    }
    
    Framebuffer::DrawString(text, text_x, text_y, 0x000000, 0xC0C0C0);
}

// Rysuje jeden poziom Menu Start (0 = główne menu, >=1 = kaskadowe submenu podkatalogu)
// i obsługuje kliki/hover jego pozycji - otwarcie podkatalogu jako następnego poziomu,
// uruchomienie apki (.ELF) albo podświetlenie już otwartego submenu.
// Zwraca true jeśli kliknięcie uruchomiło apkę - wywołujący (Render, ma dostęp do
// prywatnego Compositor::start_menu_open) powinien w tym wypadku zamknąć całe menu.
// out_consumed ustawiane na true, gdy klik trafił w jakąkolwiek pozycję menu (otwarcie/
// zamknięcie submenu też się liczy, nie tylko uruchomienie apki) - Render() na tej
// podstawie zwalnia zatrzaśnięte zbocze kliknięcia (pending_click_frames).
static bool DrawMenuLevel(int level, int mouse_x, int mouse_y, bool mouse_clicked,
                           uint32_t screen_h, int taskbar_h, int menu_w, int item_h, void* icon_bmp,
                           bool* out_consumed) {
    bool launched = false;
    StartMenuLevel* lvl = &menu_levels[level];
    int list_h = lvl->count * item_h;
    int menu_h = 8 + list_h;
    if (menu_h < 100) menu_h = 100;
    int menu_x = lvl->x;
    int menu_y = lvl->y;

    Framebuffer::DrawRect(menu_x, menu_y, menu_w, menu_h, 0xC0C0C0);
    Framebuffer::DrawRect(menu_x, menu_y, menu_w, 2, 0xFFFFFF); // highlight top
    Framebuffer::DrawRect(menu_x, menu_y, 2, menu_h, 0xFFFFFF); // highlight left
    Framebuffer::DrawRect(menu_x + menu_w - 2, menu_y, 2, menu_h, 0x000000); // shadow right
    Framebuffer::DrawRect(menu_x, menu_y + menu_h - 2, menu_w, 2, 0x000000); // shadow bottom

    // Pasek boczny "OxideOS" tylko na menu głównym - podmenu nie ma na niego miejsca/potrzeby
    int item_x_offset = 4;
    if (level == 0) {
        item_x_offset = 36;
        Framebuffer::DrawRect(menu_x + 2, menu_y + 2, 32, menu_h - 4, 0x0000A0);
        const char* os_name = "OxideOS";
        int banner_y = menu_y + menu_h - 80;
        for (int i = 0; os_name[i]; i++) {
            Framebuffer::DrawChar(os_name[i], menu_x + 14, banner_y + i * 10, 0xFFFFFF, 0x0000A0);
        }
    }

    int current_y = menu_y + 4;

    for (int i = 0; i < lvl->count; i++) {
        int bx = menu_x + item_x_offset;
        int by = current_y;
        int bw = menu_w - item_x_offset - 4;
        int bh = item_h;

        current_y += bh;

        bool is_dir = (lvl->entries[i].attributes & FS_ATTR_DIRECTORY) != 0;
        bool is_hover = (mouse_x >= bx && mouse_x <= bx + bw && mouse_y >= by && mouse_y <= by + bh);
        bool is_open_submenu = is_dir && (lvl->open_child_index == i);

        uint32_t item_bg = (is_hover || is_open_submenu) ? 0x0000A0 : 0xC0C0C0; // Win95 Hover: Dark Blue
        uint32_t item_fg = (is_hover || is_open_submenu) ? 0xFFFFFF : 0x000000; // Win95 Hover Text: White

        Framebuffer::DrawRect(bx, by, bw, bh, item_bg);

        // Ikona (na razie generyczna - domyslna ikonka okna 16x16)
        if (icon_bmp) {
            BMP::Draw(icon_bmp, bx + 4, by + (bh - 16) / 2);
        }

        // Nazwa pozycji: nazwa pliku bez rozszerzenia ".ELF" (katalogi bez zmian)
        char display_name[FS_MAX_NAME];
        int nlen = 0;
        while (lvl->entries[i].name[nlen]) nlen++;
        if (str_ends_with(lvl->entries[i].name, ".ELF")) nlen -= 4;
        for (int k = 0; k < nlen && k < FS_MAX_NAME - 1; k++) display_name[k] = lvl->entries[i].name[k];
        display_name[nlen < 0 ? 0 : nlen] = '\0';

        Framebuffer::DrawString(display_name, bx + 24, by + (bh - 8) / 2, item_fg, item_bg);

        // Katalogi dostają strzałkę ">" po prawej - sygnalizuje kaskadowe submenu
        if (is_dir) {
            Framebuffer::DrawString(">", bx + bw - 12, by + (bh - 8) / 2, item_fg, item_bg);
        }

        if (is_hover && mouse_clicked) {
            *out_consumed = true;
            if (is_dir) {
                if (level + 1 < MAX_MENU_DEPTH) {
                    if (lvl->open_child_index == i) {
                        // Ta sama pozycja kliknięta ponownie - zamknij jej submenu
                        lvl->open_child_index = -1;
                        menu_open_depth = level + 1;
                    } else {
                        RefreshMenuLevel(level + 1, lvl->paths[i]);
                        StartMenuLevel* child = &menu_levels[level + 1];
                        child->x = menu_x + menu_w;
                        int child_h = 8 + child->count * item_h;
                        if (child_h < 100) child_h = 100;
                        int child_y = by;
                        if (child_y + child_h > (int)screen_h - taskbar_h) {
                            child_y = (int)screen_h - taskbar_h - child_h;
                        }
                        if (child_y < 0) child_y = 0;
                        child->y = child_y;
                        lvl->open_child_index = i;
                        menu_open_depth = level + 2;
                    }
                }
                // Jeśli MAX_MENU_DEPTH osiągnięte - katalog pozostaje nieklikalny (brak miejsca na kolejny poziom)
            } else {
                extern void ExecAppTask(void*);
                Scheduler::CreateTask((void(*)(void*))ExecAppTask, (void*)lvl->paths[i]);
                menu_open_depth = 0;
                launched = true;
            }
        }
    }
    return launched;
}

void Compositor::Render() {
    is_rendering = true;

    // 0a. Deferred cleanup - bezpieczne zwalnianie pamięci okien między klatkami
    for (int i = 0; i < window_count; ) {
        Window* win = windows[i];
        if (win->pending_remove) {
            // Usuń z listy
            for (int j = i; j < window_count - 1; j++) {
                windows[j] = windows[j + 1];
            }
            window_count--;
            // Zwolnij pamięć
            Syscall::FreeWindowMemory(win);
            // nie inkrementuj i - ten slot zajął następny
        } else {
            i++;
        }
    }

    // Zatrzaśnij wykryte zbocze kliknięcia na kilka klatek - przy wolniejszym renderze
    // (dużo okien) mouse_left (ustawiane asynchronicznie przez przerwanie PS/2) może
    // wrócić do 0 między dwoma próbkowaniami Render(), więc samo porównanie z poprzednią
    // klatką potrafi przegapić krótkie zbocze. Licznik kasowany od razu po pierwszym
    // faktycznym zużyciu kliku w hit-teście (click_consumed), żeby nie powtórzyć akcji
    // na kolejnych klatkach.
    if (mouse_left && !prev_mouse_left) {
        pending_click_frames = 3;
    }
    prev_mouse_left = mouse_left;
    bool mouse_clicked = pending_click_frames > 0;
    bool click_consumed = false;

    // 1. Clear & Draw Background
    Framebuffer::Clear(0x008080); // Fallback / clear color
    if (bg_bmp) {
        BMP::Draw(bg_bmp, 0, 0);
    }
    
    // Draw Watermark
    uint32_t screen_w = Framebuffer::GetWidth();
    uint32_t screen_h = Framebuffer::GetHeight();
    const char* os_name = "OxideOS";
    const char* os_ver = "Wersja jadra 1.0.0 (Przejscie na Userspace)";
    
    // Text is 8x8 pixels per char
    int os_name_len = 7;
    int os_ver_len = 43;
    
    int wm_y = screen_h - 24 - 24; // Używamy sztywnej wartości (24 to wysokość paska)
    int wm_x = screen_w - (os_ver_len * 8) - 10;
    
    // Rysujemy przezroczysty tekst lekko szarym kolorem
    Framebuffer::DrawStringTransparent(os_name, screen_w - (os_name_len * 8) - 10, wm_y - 12, 0xD0D0D0);
    Framebuffer::DrawStringTransparent(os_ver, wm_x, wm_y, 0xD0D0D0);
    
    // 1.5 Window Logic (Przeciąganie okien)
    int titlebar_h = 20;
    
    // Jeśli przycisk puszczony - przerwij przeciąganie
    if (!mouse_left) {
        for (int i = 0; i < window_count; i++) {
            windows[i]->is_dragging = false;
        }
    }
    
    // Jeśli kliknięto lewym - sprawdź czy kliknięto w okno
    if (mouse_clicked && !start_menu_open) {
        // Sprawdzamy od najwyższego okna (ostatniego w tablicy)
        for (int i = window_count - 1; i >= 0; i--) {
            Window* win = windows[i];
            
            if (win->is_minimized) continue; // Pomiń zminimalizowane okna
            
            // Check [X] button (szerokość 14, wysokość 14, po prawej)
            if (mouse_x >= win->x + win->width - 18 && mouse_x <= win->x + win->width - 4 &&
                mouse_y >= win->y + 4 && mouse_y <= win->y + 18) {
                if (win->app) {
                    win->app->OnKeyPress(27); // Esc or similar?
                } else if (win->fb_buffer) {
                    Window::Event ev;
                    ev.type = 3; // Close event
                    ev.x = 0; ev.y = 0; ev.key = 0;
                    win->PushEvent(ev);
                }
                click_consumed = true;
                break; // Kończymy obsługę kliknięcia
            }

            // Check [_] minimize button (szerokość 14, wysokość 14, obok X)
            if (mouse_x >= win->x + win->width - 34 && mouse_x <= win->x + win->width - 20 &&
                mouse_y >= win->y + 4 && mouse_y <= win->y + 18) {
                win->is_minimized = true;
                click_consumed = true;
                break; // Zminimalizowano okno
            }
            
            if (mouse_x >= win->x && mouse_x <= win->x + win->width &&
                mouse_y >= win->y && mouse_y <= win->y + titlebar_h) {
                
                win->is_dragging = true;
                win->drag_start_x = mouse_x;
                win->drag_start_y = mouse_y;
                win->drag_start_win_x = win->x;
                win->drag_start_win_y = win->y;
                
                // Przenieś to okno na samą górę (na koniec tablicy)
                for (int j = i; j < window_count - 1; j++) {
                    windows[j] = windows[j + 1];
                }
                windows[window_count - 1] = win;
                click_consumed = true;
                break; // Uderzyliśmy w najwyższe okno, nie klikaj okien pod nim
            }
            
            // Sprawdź kliknięcie we wnętrze okna
            if (mouse_x >= win->x && mouse_x <= win->x + win->width &&
                mouse_y >= win->y + titlebar_h && mouse_y <= win->y + win->height) {
                if (win->app) {
                    win->app->OnMouseClick(mouse_x - win->x, mouse_y - (win->y + titlebar_h));
                } else if (win->fb_buffer) {
                    Window::Event ev;
                    ev.type = 1; // MouseClick
                    ev.x = mouse_x - (win->x + 2);
                    ev.y = mouse_y - (win->y + titlebar_h);
                    ev.key = 0;
                    win->PushEvent(ev);
                }
                // Aktywuj okno (na górę)
                for (int j = i; j < window_count - 1; j++) {
                    windows[j] = windows[j + 1];
                }
                windows[window_count - 1] = win;
                click_consumed = true;
                break;
            }
        }
    }
    
    // Aktualizuj pozycje przesuwanego okna
    bool any_dragging = false;
    for (int i = 0; i < window_count; i++) {
        Window* win = windows[i];
        if (win->is_dragging) {
            win->x = win->drag_start_win_x + (mouse_x - win->drag_start_x);
            win->y = win->drag_start_win_y + (mouse_y - win->drag_start_y);
            any_dragging = true;
        }
    }
    
    // Przekazuj MouseMove
    if (mouse_left && !any_dragging && window_count > 0 && !start_menu_open) {
        Window* top_win = nullptr;
        for (int i = window_count - 1; i >= 0; i--) {
            if (!windows[i]->is_minimized) {
                top_win = windows[i];
                break;
            }
        }
        
        if (top_win && top_win->app) {
            // Sprawdź czy kursor jest wewnątrz okna (lub pozwól na uciekanie, ale wewnątrz app body)
            top_win->app->OnMouseMove(mouse_x - top_win->x, mouse_y - (top_win->y + titlebar_h));
        }
    }
    
    // 1.6 Renderowanie Okien
    for (int i = 0; i < window_count; i++) {
        Window* win = windows[i];
        if (win->is_minimized) continue; // Pomiń zminimalizowane
        
        // Ciało okna
        Framebuffer::DrawRect(win->x, win->y, win->width, win->height, 0xC0C0C0);
        // Obramowanie okna
        Framebuffer::DrawRect(win->x, win->y, win->width, 2, 0xFFFFFF); // góra
        Framebuffer::DrawRect(win->x, win->y, 2, win->height, 0xFFFFFF); // lewo
        Framebuffer::DrawRect(win->x + win->width - 2, win->y, 2, win->height, 0x000000); // prawo
        Framebuffer::DrawRect(win->x, win->y + win->height - 2, win->width, 2, 0x000000); // dół
        
        // Pasek Tytułowy
        Framebuffer::DrawRect(win->x + 2, win->y + 2, win->width - 4, titlebar_h, 0x000080);
        
        // Rysuj Ikonę
        if (icon_bmp) {
            BMP::Draw(icon_bmp, win->x + 4, win->y + 4);
        }
        
        // Tytuł
        Framebuffer::DrawString(win->title, win->x + 24, win->y + 7, 0xFFFFFF, 0x000080);
        
        // Przycisk Zamykania [X]
        int close_x = win->x + win->width - 18;
        int close_y = win->y + 4;
        Framebuffer::DrawRect(close_x, close_y, 14, 14, 0xC0C0C0);
        Framebuffer::DrawRect(close_x, close_y, 14, 1, 0xFFFFFF);
        Framebuffer::DrawRect(close_x, close_y, 1, 14, 0xFFFFFF);
        Framebuffer::DrawRect(close_x + 13, close_y, 1, 14, 0x000000);
        Framebuffer::DrawRect(close_x, close_y + 13, 14, 1, 0x000000);
        Framebuffer::DrawString("X", close_x + 3, close_y + 3, 0x000000, 0xC0C0C0);
        
        // Przycisk Minimalizacji [_]
        int min_x = win->x + win->width - 34; // 18 + 14 + 2 = 34
        int min_y = win->y + 4;
        Framebuffer::DrawRect(min_x, min_y, 14, 14, 0xC0C0C0);
        Framebuffer::DrawRect(min_x, min_y, 14, 1, 0xFFFFFF);
        Framebuffer::DrawRect(min_x, min_y, 1, 14, 0xFFFFFF);
        Framebuffer::DrawRect(min_x + 13, min_y, 1, 14, 0x000000);
        Framebuffer::DrawRect(min_x, min_y + 13, 14, 1, 0x000000);
        Framebuffer::DrawString("_", min_x + 3, min_y + 3, 0x000000, 0xC0C0C0);
        
        // Rysuj zawartość Aplikacji
        int content_x = win->x + 2;
        int content_y = win->y + titlebar_h;
        int content_w = win->width - 4;
        int content_h = win->height - titlebar_h - 2;
        
        if (win->app) {
            win->app->OnPaint(win->x, win->y + titlebar_h, win->width, win->height - titlebar_h);
        } else if (win->fb_buffer) {
            // Rysowanie z Pamięci Dzielonej
            for (int r = 0; r < content_h; r++) {
                for (int c = 0; c < content_w; c++) {
                    Framebuffer::PutPixel(content_x + c, content_y + r, win->fb_buffer[r * content_w + c]);
                }
            }
        }
    }
    
    // 2. Logic for Start Menu
    int taskbar_h = 24;
    int btn_w = 68;
    bool clicked_start_btn = false;
    
    if (mouse_clicked) {
        if (mouse_x >= 0 && mouse_x <= btn_w && mouse_y >= (int)screen_h - taskbar_h && mouse_y <= (int)screen_h) {
            start_menu_open = !start_menu_open;
            clicked_start_btn = true;
            click_consumed = true;
            if (start_menu_open) RefreshStartMenu();
            else menu_open_depth = 0;
        }
    }
    
    // 3. Draw Taskbar
    Framebuffer::DrawRect(0, screen_h - taskbar_h, screen_w, taskbar_h, 0xC0C0C0);
    Framebuffer::DrawRect(0, screen_h - taskbar_h, screen_w, 2, 0xFFFFFF); // top highlight
    
    // Start Button (Wciśnięty/Puszczony w zależności od stanu menu)
    int btn_y = screen_h - taskbar_h + 2;
    int btn_h = taskbar_h - 4;
    DrawButton(2, btn_y, btn_w - 2, btn_h, "Start", start_menu_open);
    
    // Rysowanie otwartych okien na pasku
    int tb_x = btn_w + 4;
    for (int i = 0; i < window_count; i++) {
        Window* win = taskbar_windows[i];
        int title_len = 0;
        while(win->title[title_len]) title_len++;
        int w_btn_w = 120; // Stała szerokość
        
        if (tb_x + w_btn_w > (int)screen_w - 60) break; // Brak miejsca (zostawiamy na zegarek)
        
        bool is_top = (window_count > 0 && windows[window_count - 1] == win); // Aktywne okno (na wierzchu)
        
        // Check hover/click na taskbarze
        bool is_hover = (mouse_x >= tb_x && mouse_x <= tb_x + w_btn_w && mouse_y >= btn_y && mouse_y <= btn_y + btn_h);
        if (is_hover && mouse_clicked && !start_menu_open) {
            click_consumed = true;
            if (is_top && !win->is_minimized) {
                win->is_minimized = true;
            } else {
                win->is_minimized = false;
                // Przenieś to okno na samą górę w 'windows' (Z-Order)
                int z_index = -1;
                for (int j = 0; j < window_count; j++) {
                    if (windows[j] == win) {
                        z_index = j;
                        break;
                    }
                }
                if (z_index != -1) {
                    for (int j = z_index; j < window_count - 1; j++) {
                        windows[j] = windows[j + 1];
                    }
                    windows[window_count - 1] = win;
                }
            }
        }
        
        // Truncate title
        char trunc_title[14]; // Max ~12 chars + ...
        if (title_len > 12) {
            for(int k=0; k<9; k++) trunc_title[k] = win->title[k];
            trunc_title[9] = '.'; trunc_title[10] = '.'; trunc_title[11] = '.'; trunc_title[12] = '\0';
        } else {
            for(int k=0; k<=title_len; k++) trunc_title[k] = win->title[k];
        }
        
        DrawButton(tb_x, btn_y, w_btn_w, btn_h, trunc_title, is_top);
        tb_x += w_btn_w + 2;
    }
    
    // System Tray (Zegarek + Ikony)
    int tray_w = 54;
    if (icon_speaker) tray_w += 20; // miejsce na ikonę głośnika
    
    int tray_x = screen_w - tray_w - 2;
    Framebuffer::DrawRect(tray_x, btn_y, tray_w, btn_h, 0xC0C0C0);
    Framebuffer::DrawRect(tray_x, btn_y, tray_w, 1, 0x808080); // inner shadow top
    Framebuffer::DrawRect(tray_x, btn_y, 1, btn_h, 0x808080); // inner shadow left
    Framebuffer::DrawRect(tray_x + tray_w - 1, btn_y, 1, btn_h, 0xFFFFFF); // highlight right
    Framebuffer::DrawRect(tray_x, btn_y + btn_h - 1, tray_w, 1, 0xFFFFFF); // highlight bottom
    
    int tray_item_x = tray_x + 6;
    
    if (icon_speaker) {
        // Draw 16x16 speaker icon centered vertically
        BMP::Draw(icon_speaker, tray_item_x, btn_y + (btn_h - 16) / 2);
        tray_item_x += 20;
    }
    
    uint8_t h = RTC::GetHour();
    uint8_t m = RTC::GetMinute();
    char time_str[6] = {
        (char)('0' + (h >> 4)), (char)('0' + (h & 0x0F)), ':',
        (char)('0' + (m >> 4)), (char)('0' + (m & 0x0F)), '\0'
    };
    Framebuffer::DrawString(time_str, tray_item_x, btn_y + (btn_h - 8) / 2, 0x000000, 0xC0C0C0);
    
    // 4. Draw Start Menu (dynamiczny, pozycje z /usr/bin przez VFS::ListDirectory, rekursywnie -
    //    podkatalogi otwierają kaskadowe submenu, patrz DrawMenuLevel)
    if (start_menu_open) {
        int item_h = 24;
        int menu_w = 220;

        // Zakotwiczenie poziomu 0: lewy dolny róg, tak jak dawniej (rośnie w górę od taskbara)
        int list_h = menu_levels[0].count * item_h;
        int menu_h0 = 8 + list_h;
        if (menu_h0 < 100) menu_h0 = 100; // miejsce na pionowy baner "OxideOS"
        menu_levels[0].x = 0;
        menu_levels[0].y = screen_h - taskbar_h - menu_h0;

        bool app_launched = false;
        for (int lvl = 0; lvl < menu_open_depth && lvl < MAX_MENU_DEPTH; lvl++) {
            if (DrawMenuLevel(lvl, mouse_x, mouse_y, mouse_clicked, screen_h, taskbar_h, menu_w, item_h, icon_bmp,
                               &click_consumed)) {
                app_launched = true;
            }
        }
        if (app_launched) {
            start_menu_open = false;
        }

        // Zamknij cale menu tylko gdy klik wypadl poza WSZYSTKIMI otwartymi poziomami
        if (mouse_clicked && !clicked_start_btn && !app_launched) {
            bool in_any = false;
            for (int lvl = 0; lvl < menu_open_depth && lvl < MAX_MENU_DEPTH; lvl++) {
                int lvl_h = 8 + menu_levels[lvl].count * item_h;
                if (lvl_h < 100) lvl_h = 100;
                if (mouse_x >= menu_levels[lvl].x && mouse_x <= menu_levels[lvl].x + menu_w &&
                    mouse_y >= menu_levels[lvl].y && mouse_y <= menu_levels[lvl].y + lvl_h) {
                    in_any = true;
                    break;
                }
            }
            if (!in_any) {
                start_menu_open = false;
                menu_open_depth = 0;
                click_consumed = true;
            }
        }
    }

    // Zwolnij zatrzaśnięte zbocze kliknięcia od razu po pierwszym faktycznym zużyciu
    // (żeby ta sama fizyczna klik nie powtórzył akcji na kolejnych klatkach) - jeśli
    // nic go nie zużyło, niech dożyje jeszcze kilku klatek (patrz komentarz przy
    // pending_click_frames wyżej) zamiast zgasnąć po jednej.
    if (click_consumed) {
        pending_click_frames = 0;
    } else if (pending_click_frames > 0) {
        pending_click_frames--;
    }

    // 5. Draw Mouse
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_x >= (int)screen_w) mouse_x = screen_w - 1;
    if (mouse_y >= (int)screen_h) mouse_y = screen_h - 1;

    if (cursor_bmp) {
        // Rysuj kursor BMP z przezroczystocią (czarny = przezroczysty)
        BMPHeader* hdr = (BMPHeader*)cursor_bmp;
        BMPInfoHeader* info = (BMPInfoHeader*)((uint8_t*)cursor_bmp + sizeof(BMPHeader));
        uint8_t* pixels = (uint8_t*)cursor_bmp + hdr->data_offset;
        int cw = info->width;
        int ch = (info->height < 0) ? -info->height : info->height;
        bool flipped = (info->height > 0);
        int bpp = info->bit_count / 8;
        int stride = (cw * bpp + 3) & ~3;
        for (int row = 0; row < ch; row++) {
            int src_row = flipped ? (ch - 1 - row) : row;
            uint8_t* rdata = pixels + src_row * stride;
            for (int col = 0; col < cw; col++) {
                uint8_t b = rdata[col * bpp];
                uint8_t g = rdata[col * bpp + 1];
                uint8_t r = rdata[col * bpp + 2];
                uint8_t a = (bpp == 4) ? rdata[col * bpp + 3] : 255;
                
                // Fallback dla 24-bit BMP: czarny to przezroczysty
                if (bpp == 3 && r == 0 && g == 0 && b == 0) a = 0;
                
                if (a == 0) continue;

                int px = mouse_x + col;
                int py = mouse_y + row;
                
                if (px >= 0 && px < (int)screen_w && py >= 0 && py < (int)screen_h) {
                    if (a == 255) {
                        Framebuffer::PutPixel(px, py, (r << 16) | (g << 8) | b);
                    } else {
                        // Alpha blending
                        uint32_t bg = Framebuffer::GetPixel(px, py);
                        uint8_t bg_r = (bg >> 16) & 0xFF;
                        uint8_t bg_g = (bg >> 8) & 0xFF;
                        uint8_t bg_b = bg & 0xFF;
                        
                        uint8_t final_r = (r * a + bg_r * (255 - a)) / 255;
                        uint8_t final_g = (g * a + bg_g * (255 - a)) / 255;
                        uint8_t final_b = (b * a + bg_b * (255 - a)) / 255;
                        
                        Framebuffer::PutPixel(px, py, (final_r << 16) | (final_g << 8) | final_b);
                    }
                }
            }
        }
    } else {
        // Fallback: krzyżyk
        Framebuffer::DrawRect(mouse_x - 1, mouse_y - 5, 3, 11, 0x000000);
        Framebuffer::DrawRect(mouse_x - 5, mouse_y - 1, 11, 3, 0x000000);
        Framebuffer::DrawRect(mouse_x, mouse_y - 4, 1, 9, 0xFFFFFF);
        Framebuffer::DrawRect(mouse_x - 4, mouse_y, 9, 1, 0xFFFFFF);
    }
    
    // 6. Swap
    Framebuffer::SwapBuffers();
    is_rendering = false;
}
