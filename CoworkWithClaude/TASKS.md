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

(pusto — patrz "Zrobione" niżej za ostatnią pozycję)

## Do przegadania

- [ ] Redesign assetów — wbudowanie kluczowych plików (kursor, ikony) w binarkę kernela zamiast ładowania z FAT32 w runtime (patrz `podsumowanie_projektu.md` / decyzja z 2026-09-10)
- [ ] Przepisanie systemu plików FAT32 → **ext4** (nie exFAT — decyzja z 2026-09-11: łatwiejszy transfer z Linuksa, lepsze multi-user; dobra okazja żeby przy okazji dodać porządne locki zamiast dzisiejszego "duct tape" z `critical.h`)
- [ ] Menu Start: rekursywne submenu dla podfolderów w `/usr/bin` (dziś płaska lista — do rozszerzenia jak pojawi się realny podfolder do przetestowania)
- [ ] Skompilować i odpalić SerenityOS lokalnie (mamy klon w `.serenity/`) — rekonesans, jak wygląda dojrzały hobby-OS i jak Ladybird tam faktycznie działa
- [ ] Sieć / TCP-IP stack — zero na razie w OxideOS; wymagane zanim jakikolwiek browser (nawet coś dużo mniejszego niż Ladybird) miałby sens
- [ ] Realny model procesów z osobnymi tabelami stron per proces (dziś: jedna wspólna przestrzeń adresowa dla wszystkich tasków — zdiagnozowane 2026-09-11 przy okazji buga ze zniszczonymi oknami; per-task kernel stack już naprawiony, ale to kolejny krok w tym samym kierunku)
- [ ] "Port debugowy" — pozostałe etapy (1 i 3 zrobione 2026-09-20, patrz "Zrobione" i `PLAN_scripty_i_debug.md`): (2) inspekcja RAM-u/rejestrów/aktualnie wykonywanej instrukcji przez GDB stub QEMU (`-s -S`), (4) nagrywanie ekranu — TYLKO lokalnie na maszynie roboczej (Garuda Linux/KWin/Wayland, ASUS TUF Gaming A15), nie w chmurze.

## Zrobione

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
