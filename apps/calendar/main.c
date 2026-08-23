#include <gui.h>
#include <stddef.h>

int win_id;
uint32_t* fb;
int win_w = 260;
int win_h = 360;

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
    sys_write_file("/NOTES.DAT", (const uint8_t*)notes, sizeof(notes));
}

void DrawAppButton(int x, int y, int w, int h, const char* text) {
    gui_draw_rect(fb, win_w, x, y, w, h, 0x808080);
    gui_draw_rect(fb, win_w, x, y, w, 2, 0xFFFFFF);
    gui_draw_rect(fb, win_w, x, y, 2, h, 0xFFFFFF);
    gui_draw_rect(fb, win_w, x + w - 2, y, 2, h, 0x000000);
    gui_draw_rect(fb, win_w, x, y + h - 2, w, 2, 0x000000);
    
    int len = 0;
    while(text[len]) len++;
    
    int tx = x + (w - len * 8) / 2;
    int ty = y + (h - 8) / 2;
    gui_draw_string(fb, win_w, text, tx, ty, 0x000000, 0x808080);
}

void PaintCalendar() {
    gui_draw_rect(fb, win_w, 0, 0, win_w, win_h, 0xFFFFFF); 
    
    gui_draw_rect(fb, win_w, 0, 0, win_w, 30, 0x000080);
    gui_draw_rect(fb, win_w, 5, 5, 20, 20, 0xC0C0C0);
    gui_draw_string(fb, win_w, "<", 11, 11, 0x000000, 0xC0C0C0);
    
    gui_draw_rect(fb, win_w, win_w - 25, 5, 20, 20, 0xC0C0C0);
    gui_draw_string(fb, win_w, ">", win_w - 19, 11, 0x000000, 0xC0C0C0);
    
    char m_str[16];
    char y_str[16];
    IntToString(current_month, m_str);
    IntToString(current_year, y_str);
    
    gui_draw_string(fb, win_w, "Miesiac:", 35, 11, 0xFFFFFF, 0x000080);
    gui_draw_string(fb, win_w, m_str, 105, 11, 0xFFFFFF, 0x000080);
    gui_draw_string(fb, win_w, "Rok:", 135, 11, 0xFFFFFF, 0x000080);
    gui_draw_string(fb, win_w, y_str, 175, 11, 0xFFFFFF, 0x000080);
    
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
        
        uint32_t bg = (i + 1 == selected_day) ? 0x808080 : 0xC0C0C0;
        gui_draw_rect(fb, win_w, px, py, cell_w - 2, cell_h - 2, bg);
        
        char d_str[16];
        IntToString(i + 1, d_str);
        gui_draw_string(fb, win_w, d_str, px + 2, py + 4, 0x000000, bg);
    }
    
    int planner_y = grid_y + 6 * cell_h + 10;
    int planner_h = win_h - planner_y - 5;
    gui_draw_rect(fb, win_w, 5, planner_y, win_w - 10, planner_h, 0xFFFFCC); 
    
    if (selected_day >= 1 && selected_day <= 31) {
        char* note = notes[selected_day - 1];
        int cx = 10;
        int cy = planner_y + 10;
        for (int i = 0; note[i] != '\0'; i++) {
            if (note[i] == '\n') {
                cx = 10;
                cy += 16;
            } else {
                char str[2] = {note[i], 0};
                gui_draw_string(fb, win_w, str, cx, cy, 0x000000, 0xFFFFCC);
                cx += 8;
            }
        }
        gui_draw_rect(fb, win_w, cx, cy, 8, 16, 0x000000); 
    }
}

void OnMouseClick(int local_x, int local_y) {
    if (local_y >= 5 && local_y <= 25) {
        if (local_x >= 5 && local_x <= 25) {
            current_month--;
            if (current_month < 1) { current_month = 12; current_year--; }
            selected_day = 1;
            cursor_pos = 0;
            while(notes[selected_day-1][cursor_pos]) cursor_pos++;
        }
        if (local_x >= win_w - 25 && local_x <= win_w - 5) {
            current_month++;
            if (current_month > 12) { current_month = 1; current_year++; }
            selected_day = 1;
            cursor_pos = 0;
            while(notes[selected_day-1][cursor_pos]) cursor_pos++;
        }
    }
    
    int cell_w = 30;
    int cell_h = 20;
    int grid_x = (win_w - 7 * cell_w) / 2;
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

void OnKeyPress(char c) {
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
    
    for(int i=0; i<31; i++) {
        for(int j=0; j<128; j++) notes[i][j] = '\0';
    }
    
    PaintCalendar();
    sys_update_window(win_id);
    
    struct WindowEvent ev;
    while (1) {
        if (sys_get_event(win_id, &ev)) {
            if (ev.type == 1) { 
                OnMouseClick(ev.x, ev.y);
                PaintCalendar();
                sys_update_window(win_id);
            } else if (ev.type == 2) {
                OnKeyPress(ev.key);
                PaintCalendar();
                sys_update_window(win_id);
            }
        }
        for (volatile int i = 0; i < 10000; i++);
    }
}
