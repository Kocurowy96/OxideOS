# OxideOS — Lista Zadań

Wspólna lista zadań dla trybu pracy: w tygodniu (np. gdy Kocurowy96 jest w szkole) Claude
może samodzielnie lecieć z pozycjami stąd, a wieczorem/w weekend wspólnie testujemy
i rozbudowujemy tę listę o nowe pomysły.

## Jak z tego korzystać
- **Do zrobienia teraz** — bezpieczne do zrobienia bez nadzoru: dobrze opisane, wąskie zadania.
- **Do przegadania** — wymaga decyzji/doprecyzowania z Kocurowy96 zanim ruszy autonomicznie.
- **Zrobione** — przenosić tu po zamknięciu, z datą i krótką notką (przyda się do historii).

## Automatyzacja (od 2026-09-11)
W dni szkolne (pon-pt, 9:00 czasu polskiego) w chmurze odpala się agent Claude, który:
1. czyta ten plik, bierze pierwszą niezajętą pozycję z "Do zrobienia teraz",
2. robi jeden sensowny kawałek (buduje + testuje headless w QEMU),
3. aktualizuje ten plik (przenosi do "Zrobione" albo dopisuje notkę o postępie),
4. commituje i pushuje **tylko na gałąź `daily-work`** — main zostaje nietknięty.

Wieczorem: `git fetch && git checkout daily-work` żeby zobaczyć/przetestować co się zmieniło.
Panel: https://claude.ai/code/routines/trig_01WyfN223HwBRWFr65d18VKx

Agent po każdym uruchomieniu dopisuje też lekki raport do `daily-work-report/` (patrz
[README](../daily-work-report/README.md) i [szablon](../daily-work-report/TEMPLATE.md)) —
szybszy podgląd co zrobił niż grzebanie w logach sesji w chmurze.

**2026-09-11 wieczorem: zrobiony i zweryfikowany pierwszy pełny cykl testowy** (ręcznie
odpalony, dwie próby). Pierwsza utknęła na `rm -rf limine_dir/*` (monit bezpieczeństwa bez
nikogo do zatwierdzenia - poprawione w promptcie, patrz `HOW_WE_WORK.md`). Druga przeszła
cały cykl: zakładka Mysz zaimplementowana, zbudowana, przetestowana headless w QEMU,
zacommitowana i wypchnięta na świeżo utworzoną gałąź `daily-work` (commit `04d26f7`).
Do przetestowania w prawdziwym GUI.

---

## Do zrobienia teraz

- [ ] **Ext2 Faza 1a** (patrz `PLAN_ext2_filesystem.md` po pełny kontekst i uzasadnienie): nowy `kernel/fs/ext2.h`/`ext2.cpp` (interfejs jak `fat32.h`: `Init`/`ReadFile`/`WriteFile`/`FreeFile`/`ListDirectory`, na razie same deklaracje + `Init()`). `Init()` parsuje superblok i tablicę deskryptorów grup bloków z `disk.img` (na razie wciąż FAT32-owy — ext2 driver rozwijamy równolegle, NIE podłączać jeszcze do `VFS::`, `FAT32::` zostaje aktywny), loguje `Ext2: Initialized successfully.` analogicznie do dzisiejszego FAT32. Do testu: zbuduj lokalnie testowy obraz przez `mke2fs -t ext2 test.img` (poza `scripts/make_disk.sh`, osobny plik testowy), wgraj przez `dd`/ATA na drugi dysk QEMU (`-hdb test.img`) i zweryfikuj że `Ext2::Init()` poprawnie odczytuje pola superbloku (magic number 0xEF53, rozmiar bloku, liczba i-węzłów/grup itd.) — porównaj z tym co pokazuje `dumpe2fs test.img` na hoście. To jedyny krok Fazy 1 na liście na razie — kolejne (1b: odczyt i-węzłów, itd.) dopisujemy po ukończeniu tego, nie wszystkie na raz.

## Do przegadania

- [ ] `libgui` (`gui_draw_rect`/`gui_draw_string`) nie sprawdza granic w Y w ogóle (i `gui_draw_string` w ogóle nie sprawdza X) — znalezione 2026-09-20 przy naprawianiu Notatnika. Dziś każda apka musi sama pilnować żeby nie rysować poza `win_h`/`win_w` (Notatnik ma teraz własny scroll-clamp jako obejście), ale to systemowa dziura — bez per-process page tables przekroczenie granicy okna to zapis w cudzą pamięć (innego okna albo jądra), nie tylko wizualny glicz. Wymaga dodania `win_h` do sygnatur obu funkcji w `apps/libgui/gui.h`/`gui.c` i przejścia przez wszystkie call site'y w każdej apce — szerszy zasięg zmiany, do przegadania.
- [ ] Redesign assetów — wbudowanie kluczowych plików (kursor, ikony) w binarkę kernela zamiast ładowania z FAT32 w runtime (patrz `podsumowanie_projektu.md` / decyzja z 2026-09-10)
- [ ] Ext2 Fazy 1b-3 (odczyt i-węzłów/katalogów/plików, potem zapis, potem integracja z VFS i przełączenie `disk.img`) — pełny rozpisany plan w `PLAN_ext2_filesystem.md`, dopisywać do "Do zrobienia teraz" krok po kroku w miarę postępu Fazy 1a, nie hurtowo.
- [ ] Menu Start: rekursywne submenu dla podfolderów w `/usr/bin` (dziś płaska lista — do rozszerzenia jak pojawi się realny podfolder do przetestowania)
- [ ] Skompilować i odpalić SerenityOS lokalnie (mamy klon w `.serenity/`) — rekonesans, jak wygląda dojrzały hobby-OS i jak Ladybird tam faktycznie działa
- [ ] Sieć / TCP-IP stack — zero na razie w OxideOS; wymagane zanim jakikolwiek browser (nawet coś dużo mniejszego niż Ladybird) miałby sens
- [ ] Realny model procesów z osobnymi tabelami stron per proces (dziś: jedna wspólna przestrzeń adresowa dla wszystkich tasków — zdiagnozowane 2026-09-11 przy okazji buga ze zniszczonymi oknami; per-task kernel stack już naprawiony, ale to kolejny krok w tym samym kierunku)
- [ ] "Port debugowy" — pozostał tylko etap 4 (1, 2, 3 zrobione 2026-09-20, patrz "Zrobione" i `PLAN_scripty_i_debug.md`): nagrywanie ekranu — TYLKO lokalnie na maszynie roboczej (Garuda Linux/KWin/Wayland, ASUS TUF Gaming A15), nie w chmurze. Najmniej pilne, opcjonalne.

## Zrobione

- [x] (2026-09-20) Klawiatura (`kernel/drivers/ps2_kbd.cpp`) obsługuje teraz Shift i Caps Lock — dwie tablice scancode→ascii (unshifted/shifted), stan `shift_held`/`caps_lock` śledzony przez key-up/key-down (wcześniej sterownik w ogóle nie obsługiwał zwolnienia klawisza), wielkość liter z `shift_held XOR caps_lock`, symbole (`!@#$%^&*()_+{}:"~|<>?` itd.) tylko z Shift. Dotyczy każdej apki z wpisywaniem tekstu, nie tylko Notatnika. Przy okazji rozszerzona mapa znaków w `scripts/qmp_input.py` (nasze narzędzie testowe) o pełen zestaw symboli z Shift — przetestowane end-to-end w Notatniku ("Hello World Test 123", "Hasl0! Test#2 [a-z]=OK" renderują się poprawnie).
- [x] (2026-09-20) Notatnik przestał być surowym canvasem: dodany syscall `sys_read_file` (5, analogiczny do `sys_write_file`, kopiuje zawartość pliku do bufora appki przez `VFS::ReadFile`+`VFS::FreeFile`), menu "Zapisz"/"Otworz"/"Nowy" realnie działa (na sztywno `/notatka.txt`, bez file-pickera na razie), status w rogu menu ("Zapisano."/"Wczytano."/"Brak pliku"). Dodany też auto-scroll trzymający kursor widocznym — przy okazji naprawia realny bug: bez tego długi tekst rysowałby się poza `win_h` (biblioteka `gui_draw_string`/`gui_draw_rect` w `libgui` nie sprawdza granic w Y w ogóle — zobacz notkę niżej w "Do przegadania", to dotyczy WSZYSTKICH apek, nie tylko Notatnika). Przetestowane end-to-end (zapis→nowy→otwórz przywraca treść, 40 linii tekstu poprawnie scrolluje bez wycieku poza okno) przez `headless_interact.sh` + zrzuty ekranu.
- [x] (2026-09-20) "Port debugowy" etap 2: `scripts/gdb_inspect.sh <plik_komend_gdb> [wait_s]` — boot headless z GDB stubem QEMU (`-s`, bez `-S` żeby złapać stan "w locie" a nie od resetu), wykonuje podane komendy GDB (np. `info registers`, `bt`, `x/16i $pc`) na żywym systemie i drukuje wynik + log serialowy. Kernel.elf nie ma DWARF, ale ma symbole (mangled C++) — `bt` pokazuje realne nazwy funkcji. Przetestowane — złapało system w środku `Framebuffer::Clear`←`Compositor::Render`←`DesktopTask`, czytelny wynik. Szczegóły: `PLAN_scripty_i_debug.md`.
- [x] (2026-09-20) Sami (nie agent) naprawiliśmy trzy drobne bugi które agent dwukrotnie zgłaszał: (1) brakujący `#include <stdbool.h>` w `apps/calculator/main.c`; (2) `HELLO.ELF` link fail pod nowszym binutils — `objcopy --remove-section=.note.gnu.property` na `apps/hello/main.o` przed linkowaniem w `scripts/build.sh` (lokalnie nie było problemu, więc nie dało się tego bezpośrednio zreprodukować, ale poprawka jest bezpieczna — sekcja niepotrzebna we freestanding ELF, usunięcie jej nie zmienia zachowania, tylko usuwa źródło konfliktu segmentów); (3) `scripts/make_disk.sh` wykrywa teraz czy binarka limine jest w `limine_dir/limine` czy `limine_dir/bin/limine` zamiast zakładać z góry. Zweryfikowane pełnym czystym rebuildem i headless bootem — PASS, oba `HELLO.ELF`/`CALC.ELF` budują się.
- [x] (2026-09-20) "Port debugowy" etap 3: `scripts/qmp_input.py` (klik/scroll/klawiatura przez QMP `input-send-event`, dzieli `qmp_client.py` z etapem 1) + `scripts/headless_interact.sh <scenariusz> [katalog] [boot_wait_s]` — czyta sekwencję akcji z pliku (move/click/scroll/key/type/wait/shot) i wykonuje je na headless QEMU. W trakcie wdrażania znaleziony i naprawiony realny bug w narzędziu: klik bez przytrzymania (down+up od razu) był czasem "niewidoczny" dla `mouse_clicked = mouse_left && !prev_mouse_left` w `compositor.cpp` — fix: 150ms przytrzymania. Po fixie potwierdzone działające na żywo: zamknięcie dialogu powitalnego, otwarcie Menu Start, uruchomienie apki z niego, wpisywanie tekstu. Szczegóły: `PLAN_scripty_i_debug.md`.
- [x] (2026-09-20) "Port debugowy" etap 1: `scripts/screendump.sh` [out.png] [wait_s] — boot headless, czeka `wait_s` (domyślnie 8s), robi zrzut ekranu emulatora przez QMP `screendump` (gniazdo unix, `scripts/qmp_screendump.py`), konwertuje PPM→PNG (magick/convert), drukuje log serialowy. Pozwala "zobaczyć" GUI bez człowieka przy ekranie — przetestowane, działa (zrzut z ekranem powitalnym HELLO.ELF). Etapy 2 i 4 (GDB stub, nagrywanie) zostają w "Do przegadania".
- [x] (2026-09-15) Rozbicie `scripts/run.sh` na `scripts/build.sh` (kernel+apki), `scripts/make_disk.sh` (dysk.img+ISO z gotowych binarek) i `scripts/test_headless.sh` (build+dysk+headless boot w QEMU, log do pliku, PASS/FAIL po `FAT32: Initialized successfully` + braku "kernel panic" — kryterium z `PLAN_scripty_i_debug.md`). `scripts/run.sh` woła teraz `build.sh` + `make_disk.sh` i tylko odpala QEMU z GUI/audio — logika 1:1 przeniesiona, bez zmian w treści komend. Zweryfikowane pełnym czystym rebuildem oraz headless bootem (patrz raport z tej sesji po szczegóły i napotkane pre-existing problemy: link HELLO.ELF, brak stdbool.h w CALC.ELF, niezgodność ścieżki `limine_dir/limine` vs `limine_dir/bin/limine`).
- [x] (2026-09-14) Sprzątnięcie write-only ikon w `compositor.cpp` — usunięte `icon_programy`, `icon_clock`, `icon_folder_32` (statyczne zmienne + odpowiadające im wczytania `VFS::ReadFile` w `Init()`), bo nigdzie już nie były rysowane po przejściu na dynamiczne Menu Start. Zweryfikowane pełnym czystym buildem i headless bootem w QEMU (logi pokazują trzy mniej odczytów z FAT32 przy starcie, bez regresji).
- [x] (2026-09-11) Ustawienia: zakładka **Mysz** (czułość kursora 25-300%, `-`/`+`, kolejność Wyświetlacz → System → Dźwięk → Mysz) — nowe syscalle 11/12 (`sys_set_mouse_speed`/`sys_get_mouse_speed`), mnożnik czułości w `Mouse::HandleInterrupt` (dotyczy tylko trybu relatywnego PS/2 — QEMU domyślnie używa VMMouse w trybie absolutnym, więc efekt nie jest widoczny pod QEMU, ale realny na sprzęcie/w trybie relatywnym)
- [x] (2026-09-10) LFN w FAT32, Start Menu (styl + ikony), optymalizacja I/O dysku, redesign Panelu Sterowania (kafelki, tapety z miniaturkami)
- [x] (2026-09-11) Fix kompilacji (`icon_speaker`), tray z ikoną głośnika, sprzątanie repo (`.ELF`/`.bmp`/`.a` w gitignore)
- [x] (2026-09-11) Dynamiczne Menu Start (`FAT32::ListDirectory` + `VFS::ListDirectory`)
- [x] (2026-09-11) Ustawienia: zakładka Wyświetlacz, zakładka Dźwięk, System z żywym RAM
- [x] (2026-09-11) Fix realnego buga FAT32 (stary cache po zapisie w `SetFATEntry`)
- [x] (2026-09-11) Fix głębokiego buga schedulera — brak osobnego stosu jądra per-task, powodował niszczenie okien przy 2+ równoczesnych apkach Ring3
- [x] (2026-09-11) README.md z podziałem ról (kod — Claude, assety/testy — Kocurowy96)
