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

- [ ] Ustawienia: zakładka **Mysz** (kolejność z 2026-09-11: Wyświetlacz → System → Dźwięk → **Mysz**)
- [ ] Sprzątnąć write-only ikony w `compositor.cpp` (`icon_programy`, `icon_clock`, `icon_folder_32` — ładowane, ale nigdzie już nie rysowane po przejściu na dynamiczne Menu Start)
- [ ] Rozbić `scripts/run.sh` na osobne skrypty (ustalone 2026-09-11, planowane na najbliższą wspólną sesję): `scripts/build.sh` (sam kernel+apki), `scripts/make_disk.sh` (dysk.img+ISO z już zbudowanych binarek), `scripts/test_headless.sh` (build+dysk+boot w QEMU bez GUI, log do pliku, jasny PASS/FAIL), `scripts/run.sh` zostaje jako punkt wejścia z GUI wołający powyższe zamiast duplikować logikę

## Do przegadania

- [ ] Redesign assetów — wbudowanie kluczowych plików (kursor, ikony) w binarkę kernela zamiast ładowania z FAT32 w runtime (patrz `podsumowanie_projektu.md` / decyzja z 2026-09-10)
- [ ] Przepisanie systemu plików FAT32 → **ext4** (nie exFAT — decyzja z 2026-09-11: łatwiejszy transfer z Linuksa, lepsze multi-user; dobra okazja żeby przy okazji dodać porządne locki zamiast dzisiejszego "duct tape" z `critical.h`)
- [ ] Menu Start: rekursywne submenu dla podfolderów w `/usr/bin` (dziś płaska lista — do rozszerzenia jak pojawi się realny podfolder do przetestowania)
- [ ] Skompilować i odpalić SerenityOS lokalnie (mamy klon w `.serenity/`) — rekonesans, jak wygląda dojrzały hobby-OS i jak Ladybird tam faktycznie działa
- [ ] Sieć / TCP-IP stack — zero na razie w OxideOS; wymagane zanim jakikolwiek browser (nawet coś dużo mniejszego niż Ladybird) miałby sens
- [ ] Realny model procesów z osobnymi tabelami stron per proces (dziś: jedna wspólna przestrzeń adresowa dla wszystkich tasków — zdiagnozowane 2026-09-11 przy okazji buga ze zniszczonymi oknami; per-task kernel stack już naprawiony, ale to kolejny krok w tym samym kierunku)
- [ ] "Port debugowy" do testów — wrappery skryptowe na już istniejące możliwości QEMU (ustalone 2026-09-11, nie trzeba budować własnego protokołu): (1) zrzuty ekranu przez QMP `screendump`, (2) inspekcja RAM-u/rejestrów/aktualnie wykonywanej instrukcji przez GDB stub QEMU (`-s -S`), (3) symulacja myszy/klawiatury na "niewidocznym" ekranie przez QMP `input-send-event` do testów UI bez człowieka, (4) nagrywanie ekranu — TYLKO lokalnie na maszynie roboczej (Garuda Linux/KWin/Wayland, ASUS TUF Gaming A15), nie w chmurze. Robić etapami: (1) najpierw, (4) na końcu/opcjonalnie.

## Zrobione

- [x] (2026-09-10) LFN w FAT32, Start Menu (styl + ikony), optymalizacja I/O dysku, redesign Panelu Sterowania (kafelki, tapety z miniaturkami)
- [x] (2026-09-11) Fix kompilacji (`icon_speaker`), tray z ikoną głośnika, sprzątanie repo (`.ELF`/`.bmp`/`.a` w gitignore)
- [x] (2026-09-11) Dynamiczne Menu Start (`FAT32::ListDirectory` + `VFS::ListDirectory`)
- [x] (2026-09-11) Ustawienia: zakładka Wyświetlacz, zakładka Dźwięk, System z żywym RAM
- [x] (2026-09-11) Fix realnego buga FAT32 (stary cache po zapisie w `SetFATEntry`)
- [x] (2026-09-11) Fix głębokiego buga schedulera — brak osobnego stosu jądra per-task, powodował niszczenie okien przy 2+ równoczesnych apkach Ring3
- [x] (2026-09-11) README.md z podziałem ról (kod — Claude, assety/testy — Kocurowy96)
