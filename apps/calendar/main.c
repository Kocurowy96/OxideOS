#include <gui.h>
#include <stddef.h>

int current_month = 1;
int current_year = 2000;
int selected_day = 1;

char notes[31][128];
int cursor_pos = 0;

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

void SaveNotes() {
    // Brak pelnego wywolania pliku dla userspace do zapisu na FAT32.
    // Ale w syscall.cpp zrobiliśmy sys_write_file! Użyjemy go!
    sys_write_file("/NOTES.DAT", (const uint8_t*)notes, sizeof(notes));
}

void DrawAppButton(Window* win, int x, int y, int w, int h, const char* text) {
    gui_draw_rect(win, x, y, w, h, 0x808080);
    gui_draw_rect(win, x, y, w, 2, 0xFFFFFF);
    gui_draw_rect(win, x, y, 2, h, 0xFFFFFF);
    gui_draw_rect(win, x + w - 2, y, 2, h, 0x000000);
    gui_draw_rect(win, x, y + h - 2, w, 2, 0x000000);
    
    // Oblicz dlugosc tekstu
    int len = 0;
    while(text[len]) len++;
    
    int tx = x + (w - len * 8) / 2;
    int ty = y + (h - 16) / 2;
    gui_draw_string(win, text, tx, ty, 0x000000, 0x808080);
}

void PaintCalendar(Window* win) {
    int win_x = 0;
    int win_y = 0;
    int width = win->width;
    int height = win->height;
    
    gui_draw_rect(win, win_x, win_y, width, height, 0xFFFFFF); // Tło
    
    // Top banner
    gui_draw_rect(win, win_x, win_y, width, 30, 0x000080);
    gui_draw_rect(win, win_x + 5, win_y + 5, 20, 20, 0xC0C0C0);
    gui_draw_string(win, "<", win_x + 11, win_y + 11, 0x000000, 0xC0C0C0);
    
    gui_draw_rect(win, win_x + width - 25, win_y + 5, 20, 20, 0xC0C0C0);
    gui_draw_string(win, ">", win_x + width - 19, win_y + 11, 0x000000, 0xC0C0C0);
    
    char m_str[16];
    char y_str[16];
    IntToString(current_month, m_str);
    IntToString(current_year, y_str);
    
    gui_draw_string(win, "Miesiac:", win_x + 35, win_y + 11, 0xFFFFFF, 0x000080);
    gui_draw_string(win, m_str, win_x + 105, win_y + 11, 0xFFFFFF, 0x000080);
    gui_draw_string(win, "Rok:", win_x + 135, win_y + 11, 0xFFFFFF, 0x000080);
    gui_draw_string(win, y_str, win_x + 175, win_y + 11, 0xFFFFFF, 0x000080);
    
    // Siatka
    int start_day = GetDayOfWeek(1, current_month, current_year);
    int days = GetDaysInMonth(current_month, current_year);
    
    int cell_w = 30;
    int cell_h = 20;
    int grid_x = win_x + (width - 7 * cell_w) / 2;
    int grid_y = win_y + 50;
    
    for (int i = 0; i < days; i++) {
        int col = (start_day + i) % 7;
        int row = (start_day + i) / 7;
        
        int px = grid_x + col * cell_w;
        int py = grid_y + row * cell_h;
        
        uint32_t bg = (i + 1 == selected_day) ? 0x808080 : 0xC0C0C0;
        gui_draw_rect(win, px, py, cell_w - 2, cell_h - 2, bg);
        
        char d_str[16];
        IntToString(i + 1, d_str);
        gui_draw_string(win, d_str, px + 2, py + 4, 0x000000, bg);
    }
    
    // Planner
    int planner_y = grid_y + 6 * cell_h + 10;
    int planner_h = height - planner_y - 5;
    gui_draw_rect(win, win_x + 5, planner_y, width - 10, planner_h, 0xFFFFCC); 
    
    if (selected_day >= 1 && selected_day <= 31) {
        char* note = notes[selected_day - 1];
        int cx = win_x + 10;
        int cy = planner_y + 10;
        for (int i = 0; note[i] != '\0'; i++) {
            if (note[i] == '\n') {
                cx = win_x + 10;
                cy += 16;
            } else {
                char str[2] = {note[i], 0};
                gui_draw_string(win, str, cx, cy, 0x000000, 0xFFFFCC);
                cx += 8;
            }
        }
        gui_draw_rect(win, cx, cy, 8, 16, 0x000000); 
    }
}

void OnMouseClick(Window* win, int local_x, int local_y) {
    if (!win) return;
    if (local_y >= 5 && local_y <= 25) {
        if (local_x >= 5 && local_x <= 25) {
            current_month--;
            if (current_month < 1) { current_month = 12; current_year--; }
            selected_day = 1;
            cursor_pos = 0;
            while(notes[selected_day-1][cursor_pos]) cursor_pos++;
        }
        if (local_x >= win->width - 25 && local_x <= win->width - 5) {
            current_month++;
            if (current_month > 12) { current_month = 1; current_year++; }
            selected_day = 1;
            cursor_pos = 0;
            while(notes[selected_day-1][cursor_pos]) cursor_pos++;
        }
    }
    
    int cell_w = 30;
    int cell_h = 20;
    int grid_x = (win->width - 7 * cell_w) / 2;
    int grid_y = 50;
    
    int start_day = GetDayOfWeek(1, current_month, current_year);
    int days = GetDaysInMonth(current_month, current_year);
    
    for (int i = 0; i < days; i++) {
        int col = (start_day + i) % 7;
        int row = (start_day + i) / 7;
        int px = grid_x + col * cell_w;
        int py = grid_y + row * cell_h;
        
        if (local_x >= px && local_x <= px + cell_w - 2 &&
            local_y >= py && local_y <= py + cell_h - 2) {
            selected_day = i + 1;
            cursor_pos = 0;
            while(notes[selected_day-1][cursor_pos]) cursor_pos++;
        }
    }
}

void OnKeyPress(Window* win, char c) {
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

int main() {
    Window* win = sys_create_window(250, 80, 260, 360, "Kalendarz");
    if (!win) return 1;
    
    struct DateTime dt;
    if (sys_get_time(&dt)) {
        current_year = 2000 + dt.year;
        current_month = dt.month;
        selected_day = dt.day;
    }
    
    for(int i=0; i<31; i++) {
        for(int j=0; j<128; j++) notes[i][j] = '\0';
    }
    
    // Mozna zaladowac sys_read_file jesli napisze
    
    PaintCalendar(win);
    sys_update_window(win);
    
    Event ev;
    while (1) {
        if (sys_get_event(win, &ev)) {
            if (ev.type == EventWindowClose) {
                break;
            } else if (ev.type == EventMouseClick) {
                OnMouseClick(win, ev.mouse_x, ev.mouse_y);
                PaintCalendar(win);
                sys_update_window(win);
            } else if (ev.type == EventKeyPress) {
                OnKeyPress(win, ev.keycode);
                PaintCalendar(win);
                sys_update_window(win);
            }
        }
    }
    
    sys_exit();
    return 0;
}
