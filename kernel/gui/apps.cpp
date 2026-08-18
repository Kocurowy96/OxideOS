#include "apps.h"
#include "window.h"
#include "fb.h"
#include "../drivers/ac97.h"
#include "../fs/vfs.h"
#include "../drivers/rtc.h"
#include "../mem/pmm.h"
#include "../limine.h"

extern volatile struct limine_hhdm_request hhdm_request;
// Helper function from compositor to draw buttons
void DrawAppButton(int x, int y, int w, int h, const char* text, bool pressed) {
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

static void IntToString(int val, char* buf) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    int temp = val;
    if (temp < 0) temp = -temp;
    int pos = 0;
    char rev[16];
    while (temp > 0) {
        rev[pos++] = (temp % 10) + '0';
        temp /= 10;
    }
    if (val < 0) rev[pos++] = '-';
    
    int dpos = 0;
    while (pos > 0) {
        buf[dpos++] = rev[--pos];
    }
    buf[dpos] = '\0';
}

// --- Welcome App ---
void WelcomeApp::OnInit(Window* win) {
    window = win;
}

void WelcomeApp::OnPaint(int win_x, int win_y, int width, int height) {
    Framebuffer::DrawRect(win_x, win_y, width, height, 0xFFFFFF); // Tło białe
    
    Framebuffer::DrawString("Witaj w OxideOS!", win_x + 10, win_y + 10, 0x000000, 0xFFFFFF);
    Framebuffer::DrawString("Wersja kernela: 1.0 (Ring 3)", win_x + 10, win_y + 30, 0x000080, 0xFFFFFF);
    
    Framebuffer::DrawString("Nowosci:", win_x + 10, win_y + 60, 0x000000, 0xFFFFFF);
    Framebuffer::DrawString("- Aplikacje w trybie Userspace", win_x + 20, win_y + 80, 0x000000, 0xFFFFFF);
    Framebuffer::DrawString("- Aplikacja Ustawien w menu start", win_x + 20, win_y + 100, 0x000000, 0xFFFFFF);
    Framebuffer::DrawString("- Dzwiek AC97", win_x + 20, win_y + 120, 0x000000, 0xFFFFFF);
    
    Framebuffer::DrawString("Milego korzystania ze swiezego systemu!", win_x + 10, win_y + 160, 0x008000, 0xFFFFFF);

