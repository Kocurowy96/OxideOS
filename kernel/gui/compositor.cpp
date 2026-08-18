#include "compositor.h"
#include "fb.h"
#include "bmp.h"
#include "../drivers/ps2_mouse.h"
#include "../drivers/ac97.h"
#include "../drivers/rtc.h"
#include "application.h"
#include "apps.h"
#include "../mem/pmm.h"
#include "../limine.h"
#include <stddef.h>

inline void* operator new(size_t, void* p) { return p; }
inline void* operator new[](size_t, void* p) { return p; }

#include "../fs/vfs.h"
#include "../serial.h"

extern volatile struct limine_hhdm_request hhdm_request;

static void* bg_bmp = nullptr;
void* Compositor::icon_bmp = nullptr;

Window* Compositor::windows[MAX_WINDOWS];
Window* Compositor::taskbar_windows[MAX_WINDOWS];
int Compositor::window_count = 0;

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
    if (index != -1) {
        for (int i = index; i < window_count - 1; i++) {
            windows[i] = windows[i + 1];
        }
    }
    
    int tb_index = -1;
    for (int i = 0; i < window_count; i++) {
        if (taskbar_windows[i] == win) {
            tb_index = i;
            break;
        }
    }
    if (tb_index != -1) {
        for (int i = tb_index; i < window_count - 1; i++) {
            taskbar_windows[i] = taskbar_windows[i + 1];
        }
    }
    
    if (index != -1 && tb_index != -1) {
        window_count--;
    }
}

Window* Compositor::GetWindowById(int id) {
    for (int i = 0; i < window_count; i++) {
        if (windows[i]->id == id) {
            return windows[i];
        }
    }
    return nullptr;
}

void Compositor::Init() {
    char bg_name[32] = "/bg.bmp";
    uint8_t* cfg_buf = nullptr;
    uint32_t cfg_size = 0;
    
    // Próba wczytania konfiguracji tapety
    if (VFS::ReadFile("/DOCS/CONFIG.DAT", &cfg_buf, &cfg_size)) {
        if (cfg_size > 0 && cfg_size < 32) {
            for (uint32_t i = 0; i < cfg_size; i++) {
                bg_name[i] = (char)cfg_buf[i];
            }
            bg_name[cfg_size] = '\0';
        }
    }
    
    uint8_t* buffer = nullptr;
    uint32_t size = 0;
    
    if (VFS::ReadFile(bg_name, &buffer, &size)) {
        bg_bmp = buffer;
        SerialPort::WriteString("Compositor: Loaded configured wallpaper!\n");
    } else {
        // Fallback
        if (VFS::ReadFile("/bg.bmp", &buffer, &size)) {
            bg_bmp = buffer;
            SerialPort::WriteString("Compositor: Loaded default bg.bmp\n");
        } else {
            SerialPort::WriteString("Compositor: Failed to load any wallpaper.\n");
        }
    }

    buffer = nullptr;
    size = 0;
    if (VFS::ReadFile("/icon.bmp", &buffer, &size)) {
        icon_bmp = buffer;
        SerialPort::WriteString("Compositor: Loaded icon.bmp from VFS!\n");
    } else {
        SerialPort::WriteString("Compositor: Failed to load icon.bmp from VFS.\n");
    }
}

void Compositor::HandleKeyPress(char c) {
    if (window_count > 0) {
        Window* top_win = windows[window_count - 1];
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

void Compositor::Render() {
    // 0. Update Input State
    bool mouse_clicked = (mouse_left && !prev_mouse_left);
    prev_mouse_left = mouse_left;

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
            
            // Check [X] button (szerokość 14, wysokość 14, po prawej)
            if (mouse_x >= win->x + win->width - 18 && mouse_x <= win->x + win->width - 4 &&
                mouse_y >= win->y + 4 && mouse_y <= win->y + 18) {
                RemoveWindow(win);
                break; // Usunięto okno, kończymy obsługę kliknięcia
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
        Window* top_win = windows[window_count - 1];
        if (top_win->app) {
            // Sprawdź czy kursor jest wewnątrz okna (lub pozwól na uciekanie, ale wewnątrz app body)
            top_win->app->OnMouseMove(mouse_x - top_win->x, mouse_y - (top_win->y + titlebar_h));
        }
    }
    
    // 1.6 Renderowanie Okien
    for (int i = 0; i < window_count; i++) {
        Window* win = windows[i];
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
            is_top = true;
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
    
    // Zegarek
    int clock_w = 54;
    int clock_x = screen_w - clock_w - 2;
    Framebuffer::DrawRect(clock_x, btn_y, clock_w, btn_h, 0xC0C0C0);
    Framebuffer::DrawRect(clock_x, btn_y, clock_w, 1, 0x808080); // inner shadow top
    Framebuffer::DrawRect(clock_x, btn_y, 1, btn_h, 0x808080); // inner shadow left
    Framebuffer::DrawRect(clock_x + clock_w - 1, btn_y, 1, btn_h, 0xFFFFFF); // highlight right
    Framebuffer::DrawRect(clock_x, btn_y + btn_h - 1, clock_w, 1, 0xFFFFFF); // highlight bottom
    
    uint8_t h = RTC::GetHour();
    uint8_t m = RTC::GetMinute();
    char time_str[6] = {
        (char)('0' + (h >> 4)), (char)('0' + (h & 0x0F)), ':',
        (char)('0' + (m >> 4)), (char)('0' + (m & 0x0F)), '\0'
    };
    Framebuffer::DrawString(time_str, clock_x + 8, btn_y + (btn_h - 8) / 2, 0x000000, 0xC0C0C0);
    
    // 4. Draw Start Menu
    static bool programs_hovered_persistent = false;
    static uint32_t hover_frames = 0;
    
    if (start_menu_open) {
        int menu_w = 160;
        int menu_h = 240;
        int menu_y = screen_h - taskbar_h - menu_h;
        
        Framebuffer::DrawRect(0, menu_y, menu_w, menu_h, 0xC0C0C0);
        Framebuffer::DrawRect(0, menu_y, menu_w, 2, 0xFFFFFF); // highlight top
        Framebuffer::DrawRect(0, menu_y, 2, menu_h, 0xFFFFFF); // highlight left
        Framebuffer::DrawRect(menu_w - 2, menu_y, 2, menu_h, 0x000000); // shadow right
        Framebuffer::DrawRect(0, menu_y + menu_h - 2, menu_w, 2, 0x000000); // shadow bottom
        
        // Pasek Boczny
        Framebuffer::DrawRect(2, menu_y + 2, 24, menu_h - 4, 0x000080);
        
        const char* os_name = "OxideOS";
        int banner_y = menu_y + menu_h - 80;
        for (int i = 0; os_name[i]; i++) {
            Framebuffer::DrawChar(os_name[i], 10, banner_y + i * 10, 0xFFFFFF, 0x000080);
        }
        
        const char* menu_items[] = {
            "Programy >",
            "Kalkulator",
            "Ustawienia"
        };
        
        int item_y = menu_y + 4;
        bool any_program_hovered = false;
        
        for (int i = 0; i < 3; i++) {
            int bx = 28;
            int by = item_y + i * 22; // Zmniejszono odstęp z 26 na 22
            int bw = menu_w - 32;
            int bh = 20; // Zmniejszono wysokość przycisków z 24 na 20
            
            bool is_hover = (mouse_x >= bx && mouse_x <= bx + bw && mouse_y >= by && mouse_y <= by + bh);
            bool is_pressed = (is_hover && mouse_left);
            
            if (i == 0 && is_hover) any_program_hovered = true;
            
            DrawButton(bx, by, bw, bh, menu_items[i], is_pressed);
            
            if (is_hover && mouse_clicked) {
                extern void ExecAppTask(void*);
                
                if (i == 1) { // Kalkulator
                    Scheduler::CreateTask((void(*)(void*))ExecAppTask, (void*)"/CALC.ELF");
                    start_menu_open = false;
                } else if (i == 2) { // Notatnik
                    Scheduler::CreateTask((void(*)(void*))ExecAppTask, (void*)"/SETTINGS.ELF"); // Tymczasowo Notatnik odpala Ustawienia dla testu, potem można usunąć lub podmienić
                    start_menu_open = false;
                }
            }
        }
        
        if (any_program_hovered) {
            hover_frames++;
            if (hover_frames > 45) programs_hovered_persistent = true; // Zwiększono czas otwarcia podmenu na ok 1.5s
        } else {
            hover_frames = 0;
        }
        
        int sub_w = 120;
        int sub_h = 60;
        int sub_x = menu_w - 2;
        int sub_y = item_y; // na wysokosci "Programy >"
        
        bool in_sub = false;
        if (programs_hovered_persistent) {
            in_sub = (mouse_x >= sub_x && mouse_x <= sub_x + sub_w && mouse_y >= sub_y && mouse_y <= sub_y + sub_h);
            
            if (!any_program_hovered && !in_sub) {
                programs_hovered_persistent = false;
            } else {
                Framebuffer::DrawRect(sub_x, sub_y, sub_w, sub_h, 0xC0C0C0);
                Framebuffer::DrawRect(sub_x, sub_y, sub_w, 2, 0xFFFFFF);
                Framebuffer::DrawRect(sub_x, sub_y, 2, sub_h, 0xFFFFFF);
                Framebuffer::DrawRect(sub_x + sub_w - 2, sub_y, 2, sub_h, 0x000000);
                Framebuffer::DrawRect(sub_x, sub_y + sub_h - 2, sub_w, 2, 0x000000);
                
                const char* sub_items[] = { "Kalendarz", "Paint" };
                for (int j = 0; j < 2; j++) {
                    int bx = sub_x + 4;
                    int by = sub_y + 4 + j * 22;
                    int bw = sub_w - 8;
                    int bh = 20;
                    
                    bool s_hover = (mouse_x >= bx && mouse_x <= bx + bw && mouse_y >= by && mouse_y <= by + bh);
                    bool s_pressed = (s_hover && mouse_left);
                    DrawButton(bx, by, bw, bh, sub_items[j], s_pressed);
                    
                    if (s_hover && mouse_clicked) {
                        if (j == 0) { // Kalendarz
                            Scheduler::CreateTask((void(*)(void*))ExecAppTask, (void*)"/CALENDAR.ELF");
                            start_menu_open = false;
                            programs_hovered_persistent = false;
                        } else if (j == 1) { // Paint
                            Scheduler::CreateTask((void(*)(void*))ExecAppTask, (void*)"/PAINT.ELF");
                            start_menu_open = false;
                            programs_hovered_persistent = false;
                        }
                    }
                }
            }
        }
        
        if (mouse_clicked && !clicked_start_btn) {
            bool in_menu = (mouse_x >= 0 && mouse_x <= menu_w && mouse_y >= menu_y && mouse_y <= menu_y + menu_h);
            if (!in_menu && !in_sub) {
                start_menu_open = false;
                programs_hovered_persistent = false;
            }
        }
    } else {
        programs_hovered_persistent = false;
        hover_frames = 0;
    }
    
    // 5. Draw Mouse
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_x >= (int)screen_w) mouse_x = screen_w - 1;
    if (mouse_y >= (int)screen_h) mouse_y = screen_h - 1;
    
    // simple crosshair
    Framebuffer::DrawRect(mouse_x - 1, mouse_y - 5, 3, 11, 0x000000);
    Framebuffer::DrawRect(mouse_x - 5, mouse_y - 1, 11, 3, 0x000000);
    Framebuffer::DrawRect(mouse_x, mouse_y - 4, 1, 9, 0xFFFFFF);
    Framebuffer::DrawRect(mouse_x - 4, mouse_y, 9, 1, 0xFFFFFF);
    
    // 6. Swap
    Framebuffer::SwapBuffers();
}
