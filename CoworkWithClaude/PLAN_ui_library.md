# Plan: biblioteka UI dla apek systemowych ("WinForms-lite")

Ustalone 2026-09-26/27 w nocy, po naprawieniu Kalendarza/WinVer. Powód: kilka apek (Kalkulator,
Ustawienia, Kalendarz, WinVer) ma niemal identyczne, skopiowane funkcje (`DrawAppButton`,
`itoa`/`IntToString`) — a co ważniejsze, każda **osobno i ręcznie** testuje trafienie kliknięcia
w prostokąt przycisku w swoim `OnMouseClick` (tablica `buttons[]` + pętla `if (local_x >= bx &&
...)`). To ta druga część jest bardziej podatna na błędy i bardziej warta wspólnego rozwiązania
niż samo rysowanie.

## Kluczowa decyzja: prawdziwe kontrolki z dyspozytorem zdarzeń, nie tylko wspólne funkcje rysujące

Rozważone dwie opcje:
1. **Same funkcje rysujące** (`gui_draw_button()` wołane ręcznie co klatkę, tak jak dziś, tylko
   bez kopiowania kodu) — mniejszy krok, ale nie rozwiązuje prawdziwego źródła powtarzalności/
   błędów: każda apka nadal osobno pisze swój hit-test.
2. **Kontrolki + `Form`-dyspozytor** ("WinForms-lite") — `Control` (prostokąt + wskaźniki na
   funkcje `render`/`on_click`), `Form` (lista kontrolek + wspólna pętla zdarzeń), która sama
   robi `sys_get_event`→hit-test→wywołanie callbacku→przerysowanie. Apka dodaje kontrolki raz
   przy starcie i pisze tylko logikę biznesową w callbackach — nie musi już nigdy ręcznie
   testować współrzędnych kliknięcia.

**Wybrane: opcja 2.** Uzasadnienie: opcja 1 to półśrodek, który i tak trzeba by było przerobić
później, gdyby okazało się (a prawdopodobnie by się okazało) że prawdziwym bólem jest hit-testing,
nie duplikacja rysowania. Lepiej zdecydować o docelowej architekturze teraz i budować ją od razu
przyrostowo, niż budować coś węższego i migrować dwa razy. W C (apki są w czystym C, bez klas)
realizuje się to strukturami + wskaźnikami na funkcje — sprawdzony, prosty wzorzec (podobny do
starych toolkitów w stylu Amiga/GEM czy dzisiejszych immediate-mode-adjacent bibliotek typu
Nuklear/microui), nie wymaga przepisywania reszty kernela/syscalli.

## Szkic API (do doprecyzowania przy Fazie 1, nie ustalone na twardo teraz)

```c
typedef struct Control Control;

typedef void (*ControlRenderFn)(Control* self, uint32_t* fb, int win_w);
typedef void (*ControlClickFn)(Control* self);

struct Control {
    int x, y, w, h;
    char text[64];
    ControlRenderFn render;   // rysuje siebie (tlo/ramka/tekst)
    ControlClickFn on_click;  // wywolywane przez Form po trafieniu kliku w (x,y,w,h)
    void* user_data;          // wskaznik do stanu apki (np. biezaca wartosc kalkulatora)
};

typedef struct {
    int win_id;
    uint32_t* fb;
    int win_w, win_h;
    Control controls[32];     // limit na start - stala, maly narzut, pasuje do rozmiaru apek dzis
    int control_count;
} Form;

Control* gui_form_add_button(Form* form, int x, int y, int w, int h, const char* text,
                              ControlClickFn on_click, void* user_data);
Control* gui_form_add_label(Form* form, int x, int y, const char* text);
void gui_form_paint(Form* form);   // czysci tlo + rysuje wszystkie kontrolki po kolei
void gui_form_run(Form* form);     // petla: sys_get_event -> hit-test -> on_click -> gui_form_paint -> sys_update_window
```

Docelowo (NIE w Fazie 1, dopisywać kolejno w miarę potrzeb konkretnych apek): pole tekstowe z
kursorem (do Notatnika/pola notatek Kalendarza), lista/tabela (do listy zadań w Menedżerze
Zadań), suwak (do głośności/czułości myszy w Ustawieniach zamiast dzisiejszych `-`/`+`),
zakładki/sidebar (do Ustawień zamiast ręcznej listy). Nie projektować szczegółów tych kontrolek
teraz — dopiero gdy faktycznie ruszy ich migracja.

## Powiązanie z istniejącym long-term itemem: brak sprawdzania granic w `libgui`

`gui_draw_rect`/`gui_draw_string` w `apps/libgui/gui.c` nie sprawdzają granic w Y w ogóle (i
`gui_draw_string` w ogóle nie sprawdza X) — znalezione 2026-09-20, dotąd w "Do przegadania" bez
ruchu, bo wymagałoby dodania `win_h` do sygnatur i przejścia przez wszystkie call site'y w każdej
apce (szerszy zasięg zmiany). **Skoro i tak budujemy nową warstwę nad tymi funkcjami, to naturalny
moment żeby to naprawić przy okazji** — `Control::render` i tak dostaje `win_w`, można też dać mu
`win_h` (albo cały wskaźnik na `Form`) i naprawić bounds-checking na tym poziomie, zamiast osobnej
sesji dotykającej te same pliki po raz drugi. Do potwierdzenia przy starcie Fazy 1, nie teraz.

## Fazy

**Faza 0 — ten dokument + decyzja architektury.** Zrobione.

**Faza 1 — szkielet biblioteki + Kalkulator jako dowód koncepcji:**
- `apps/libgui/widgets.h`/`widgets.c` (nowy plik, nie rozrasta się `gui.c`): `Control`/`Form`,
  `gui_form_add_button`/`add_label`, `gui_form_paint`, `gui_form_run`, styl przycisku 1:1 jak
  dzisiejszy `DrawAppButton` (żeby wizualnie nic się nie zmieniło, tylko mechanizm pod spodem).
- Przepisanie `apps/calculator/main.c` na `Form`/`Control` — 16 przycisków cyfr/operatorów +
  wyświetlacz jako `Label` aktualizowany w callbackach. `_start()` sprowadza się do zbudowania
  `Form` raz i wywołania `gui_form_run()` zamiast własnej pętli `while(1)`.
- Weryfikacja: `headless_interact.sh` — seria kliknięć odtwarzająca konkretne działanie (np.
  "7 + 3 =" → wynik "10"), zrzuty ekranu przed/po, porównanie z dzisiejszym zachowaniem
  Kalkulatora (identyczny wygląd, identyczna logika, tylko inny mechanizm w środku).

**Faza 2+ (dopisywać po zamknięciu Fazy 1, nie z góry) — migracja kolejnych apek jedna po
drugiej:** Ustawienia (największa, ma zakładki — dobra okazja żeby dodać kontrolkę
zakładek/sidebar), Kalendarz (siatka dni to w istocie siatka przycisków, podobna do
Kalkulatora), WinVer (najprostsza — tylko przycisk OK + etykiety), Notatnik/pole notatek w
Kalendarzu (wymaga najpierw kontrolki pola tekstowego z kursorem — nowy typ kontrolki, nie
tylko migracja).

## Jak to wejdzie do TASKS.md

Do "Do zrobienia teraz" trafia na start tylko **Faza 1** (szkielet + Kalkulator) — jeden,
kompletny, samodzielnie weryfikowalny krok. Kolejne apki dopisywane pojedynczo po zamknięciu
poprzedniej, nie hurtowo — ten sam wzorzec co ext2/sieć.
