#include "compositor.h"
#include "fb.h"
#include "bmp.h"
#include "../drivers/ps2_mouse.h"
#include "../limine.h"
#include "../drivers/ac97.h"

#include "../fs/vfs.h"
#include "../serial.h"

static void* bg_bmp = nullptr;
void* Compositor::icon_bmp = nullptr;

Window* Compositor::windows[MAX_WINDOWS];
int Compositor::window_count = 0;

bool Compositor::AddWindow(Window* win) {
    if (window_count >= MAX_WINDOWS) return false;
    windows[window_count++] = win;
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
        window_count--;
    }
}

void Compositor::Init() {
    uint8_t* buffer = nullptr;
    uint32_t size = 0;
    
    if (VFS::ReadFile("/bg.bmp", &buffer, &size)) {
        bg_bmp = buffer;
        SerialPort::WriteString("Compositor: Loaded bg.bmp from VFS!\n");
    } else {
        SerialPort::WriteString("Compositor: Failed to load bg.bmp from VFS.\n");
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

bool Compositor::prev_mouse_left = false;
bool Compositor::start_menu_open = false;

void Compositor::Render() {
    // 0. Update Input State
    bool mouse_clicked = (mouse_left && !prev_mouse_left);
    prev_mouse_left = mouse_left;

    // 1. Clear & Draw Background
    Framebuffer::Clear(0x008080); // Fallback / clear color
    if (bg_bmp) {
        BMP::Draw(bg_bmp, 0, 0);
    }
    
    uint32_t screen_w = Framebuffer::GetWidth();
    uint32_t screen_h = Framebuffer::GetHeight();
    
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
        }
    }
    
    // Aktualizuj pozycje przesuwanego okna
    for (int i = 0; i < window_count; i++) {
        Window* win = windows[i];
        if (win->is_dragging) {
            win->x = win->drag_start_win_x + (mouse_x - win->drag_start_x);
            win->y = win->drag_start_win_y + (mouse_y - win->drag_start_y);
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
    }
    
    // 2. Logic for Start Menu
    int taskbar_h = 22;
    int btn_w = 68;
    
    if (mouse_clicked) {
        if (mouse_x >= 0 && mouse_x <= btn_w && mouse_y >= (int)screen_h - taskbar_h && mouse_y <= (int)screen_h) {
            start_menu_open = !start_menu_open;
        } else if (start_menu_open) {
            int menu_w = 200;
            int menu_h = 300;
            int menu_y = screen_h - taskbar_h - menu_h;
            
            if (mouse_x >= 40 && mouse_x <= menu_w && mouse_y >= menu_y + 130 && mouse_y <= menu_y + 160) {
                AC97::PlaySquareWave();
                start_menu_open = false;
            } else if (mouse_x >= 40 && mouse_x <= menu_w && mouse_y >= menu_y + 160 && mouse_y <= menu_y + 190) {
                uint8_t* wav_buffer = nullptr;
                uint32_t wav_size = 0;
                if (VFS::ReadFile("/STARTUP.WAV", &wav_buffer, &wav_size)) {
                    AC97::PlayWAV(wav_buffer);
                }
                start_menu_open = false;
            } else {
                start_menu_open = false;
            }
        }
    }
    
    // 3. Draw Taskbar
    Framebuffer::DrawRect(0, screen_h - taskbar_h, screen_w, taskbar_h, 0xC0C0C0);
    Framebuffer::DrawRect(0, screen_h - taskbar_h, screen_w, 2, 0xFFFFFF); // top highlight
    
    // Start Button (Wciśnięty/Puszczony w zależności od stanu menu)
    int btn_y = screen_h - taskbar_h + 2;
    int btn_h = taskbar_h - 4;
    if (start_menu_open) {
        Framebuffer::DrawRect(2, btn_y, btn_w - 2, btn_h, 0xC0C0C0); // Jasny, puszczony
        Framebuffer::DrawRect(2, btn_y, btn_w - 2, 1, 0x000000); // inner shadow top
        Framebuffer::DrawRect(2, btn_y, 1, btn_h, 0x000000); // inner shadow left
    } else {
        Framebuffer::DrawRect(2, btn_y, btn_w - 2, btn_h, 0xC0C0C0); // Jasny, puszczony
        Framebuffer::DrawRect(2, btn_y, btn_w - 2, 1, 0xFFFFFF); // highlight top
        Framebuffer::DrawRect(2, btn_y, 1, btn_h, 0xFFFFFF); // highlight left
        Framebuffer::DrawRect(btn_w - 1, btn_y, 1, btn_h, 0x000000); // shadow right
        Framebuffer::DrawRect(btn_w, btn_y, 1, btn_h, 0x000000); // shadow right
        Framebuffer::DrawRect(2, btn_y + btn_h - 1, btn_w - 2, 1, 0x000000); // shadow dół
    }
    
    Framebuffer::DrawString("OxideOS", 7 + (start_menu_open ? 1 : 0), screen_h - taskbar_h + 7 + (start_menu_open ? 1 : 0), 0x000000, 0xC0C0C0);
    
    // 4. Draw Start Menu
    if (start_menu_open) {
        int menu_w = 200;
        int menu_h = 300;
        int menu_y = screen_h - taskbar_h - menu_h;
        
        Framebuffer::DrawRect(0, menu_y, menu_w, menu_h, 0xC0C0C0);
        Framebuffer::DrawRect(0, menu_y, menu_w, 2, 0xFFFFFF); // highlight top
        Framebuffer::DrawRect(0, menu_y, 2, menu_h, 0xFFFFFF); // highlight left
        Framebuffer::DrawRect(menu_w - 2, menu_y, 2, menu_h, 0x000000); // shadow right
        Framebuffer::DrawRect(0, menu_y + menu_h - 2, menu_w, 2, 0x000000); // shadow bottom
        
        // Pasek Boczny z gradientem / kolorem
        Framebuffer::DrawRect(2, menu_y + 2, 30, menu_h - 4, 0x000080);
        
        // Items
        Framebuffer::DrawString("OxideOS (WIP)", 45, menu_y + 10, 0x000000, 0xC0C0C0);
        
        Framebuffer::DrawRect(40, menu_y + 35, menu_w - 44, 2, 0x808080); // Separator
        Framebuffer::DrawRect(40, menu_y + 36, menu_w - 44, 1, 0xFFFFFF);
        
        Framebuffer::DrawString("1. Kalkulator", 50, menu_y + 50, 0x000000, 0xC0C0C0);
        Framebuffer::DrawString("2. Terminal", 50, menu_y + 80, 0x000000, 0xC0C0C0);
        Framebuffer::DrawString("3. Pasjans", 50, menu_y + 110, 0x000000, 0xC0C0C0);
        Framebuffer::DrawString("4. Dzwiek Testowy", 50, menu_y + 140, 0x000000, 0xC0C0C0);
        Framebuffer::DrawString("5. Odtworz WAV", 50, menu_y + 170, 0x000000, 0xC0C0C0);
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
