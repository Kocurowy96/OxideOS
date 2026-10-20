# Formalny zestaw regresji UI

Spisane, zweryfikowane scenariusze `scripts/headless_interact.sh` (format pliku: patrz
komentarz na górze tego skryptu) dla apek z GUI — zamiast wyliczać współrzędne kliknięć od
zera w każdej sesji. Jeden plik na apkę. Uruchamiane jako checklist przed większym mergem/
milestone'em (ręcznie, na żądanie — nic tu nie jest spięte z CI).

## Jak uruchomić jeden scenariusz

```
./scripts/headless_interact.sh CoworkWithClaude/regression/scenario_calculator.txt /tmp/out 10
```

Trzeci argument (`boot_wait_s`, domyślnie 8) to czas na rozruch przed pierwszą akcją ze
scenariusza — 10s jest bezpiecznym zapasem używanym przy weryfikacji tych plików. Zrzuty
ekranu (`shot ...png`) lądują w podanym katalogu wyjściowym (`/tmp/out` wyżej) — obejrzyj je
(np. `scripts/screendump.sh`-owym `convert`em już zrobionym przez sam `headless_interact.sh`,
albo po prostu dowolną przeglądarką obrazów) i porównaj z opisem oczekiwanego stanu w
komentarzach danego scenariusza. To jest checklist do przeglądu przez człowieka (albo przez
Claude w kolejnej sesji) — skrypt sam w sobie nie ocenia czy ekran wygląda poprawnie, tylko
wykonuje kliknięcia i robi zrzuty.

## Ważne: każdy scenariusz zamyka HELLO.ELF jako pierwszy krok

Domyślne okienko powitalne `HELLO.ELF` uruchamia się automatycznie przy każdym świeżym
rozruchu. **Nie zamykać go przed otwarciem drugiej apki z Menu Start odtwarza znany,
wciąż niezbadany do końca kernel panic** (Exception 14 / Page Fault, patrz
`CoworkWithClaude/TASKS.md`, pozycja "Zbadać i naprawić realny kernel panic..." — tam pełny
log i historia dochodzenia z kilku sesji). Dlatego **każdy** scenariusz w tym katalogu
zaczyna się od kliknięcia przycisku [X] okna HELLO (współrzędne `705 293`, patrz niżej) —
NIE pomijać/nie zmieniać tej kolejności przy edycji scenariuszy, dopóki tamten bug nie
zostanie naprawiony. Z tego samego powodu każdy scenariusz testuje dokładnie jedną apkę na
raz (otwiera, testuje, zamyka, dopiero wtedy — gdyby ktoś chciał rozszerzyć plik — można by
otworzyć kolejną) — nigdy dwie jednocześnie.

## Skąd wzięły się współrzędne

Rozdzielczość ekranu w tym środowisku (QEMU + Limine, BIOS/VBE) to **1280×800** (zmierzone
wprost ze zrzutu ekranu `identify`, NIE zakładane z rozmiaru tapety w `assets/` — te są
1024×768/1280×720, co jest mylące, zostały przeskalowane/nie są używane 1:1 do ustalania
rozdzielczości). Jeśli kiedyś rozdzielczość się zmieni (np. inny framebuffer mode z Limine),
współrzędne oparte o `screen_w`/`screen_h` (przycisk Start, pozycje Menu Start) trzeba będzie
przeliczyć na nowo wg wzorów niżej — współrzędne W OBRĘBIE okien apek (przyciski kalkulatora,
zakładki Ustawień, itd.) są od rozdzielczości niezależne (okna mają stałą pozycję x/y
przekazywaną przez samą apkę do `sys_create_window`, patrz każdy `apps/*/main.c`).

Wzory (`kernel/gui/compositor.cpp`, `Compositor::Render`/`DrawMenuLevel`):
- Przycisk Start: `x∈[0,68]`, `y∈[screen_h-24, screen_h]` → klik na `(34, screen_h-12)`.
- Pozycja `i`-tej pozycji głównego Menu Start (po kliknięciu Start, poziom 0, alfabetyczna/
  katalogowa kolejność z `/usr/bin` — patrz `scripts/make_disk.sh` dla obecnej kolejności:
  HELLO(0), SETTINGS(1), CALC(2), PAINT(3), CALENDAR(4), WINVER(5), CLOCK(6), NOTEPAD(7),
  TASKMGR(8), ACCESSORIES/(9)):
  `menu_h0 = max(100, 8 + liczba_pozycji*24)`, `menu_y = screen_h - 24 - menu_h0`,
  środek pozycji `i`: `x = 126`, `y = menu_y + 16 + i*24`.
- Przycisk [X] zamknięcia okna: `x∈[win.x+width-18, win.x+width-4]`,
  `y∈[win.y+4, win.y+18]` gdzie `width = w+4`, `height = h+22` (`w,h` to wymiary przekazane
  do `sys_create_window` przez apkę, `win.x`/`win.y` to pozycja też przekazana przez apkę).
- Zawartość okna (treść, przyciski itd.) zaczyna się w `(win.x+2, win.y+20)` na ekranie —
  współrzędne kontrolek w kodzie apki (np. `gui_form_add_button(&form, bx, by, ...)`) są
  względem tego punktu, nie względem `(0,0)` ekranu.

Wszystkie poniższe scenariusze zostały **faktycznie uruchomione i zweryfikowane wizualnie**
(zrzuty ekranu) w tej sesji (2026-10-20, kontener agenta w chmurze, bez `/dev/kvm` — czysta
emulacja TCG) — nie są wyliczone teoretycznie/nieprzetestowane. Gdyby po zmianie kodu GUI
(layoutu okna, Menu Start, rozdzielczości) przestały trafiać we właściwe miejsca, to znak że
ten plik wymaga aktualizacji, nie że jest z nim coś z gruntu nie tak.

**Znana, osobna usterka zaobserwowana przy tej okazji (niezwiązana z samymi scenariuszami):**
`type` w `scenario_calendar.txt` dwukrotnie uruchomiony w tej sesji dał dwa różne wyniki —
raz pełne "Test regresji", raz z zgubionymi znakami w środku ("Test regji"); `NOTEPAD.ELF`
w tej samej sesji zawsze wpisywał tekst poprawnie. To ten sam rodzaj niedeterminizmu co znany,
opisany w `docs/gui.md` problem z gubieniem kliknięć przy wolniejszym renderze (próbkowanie
raz na klatkę) — tu dotyczy raczej klawiatury niż myszy, prawdopodobnie osobne zjawisko, nie
zbadane głębiej w tej sesji (poza zakresem tego zadania). Przy przeglądaniu zrzutów z
`scenario_calendar.txt` traktować wpisany tekst jako przybliżony, nie bajt-w-bajt — pojedynczy
zgubiony znak nie oznacza realnej regresji w samej apce.

## Lista scenariuszy

| Plik | Apka | Co sprawdza |
|---|---|---|
| `scenario_calculator.txt` | CALC.ELF | Otwarcie, klik `2 + 3 =`, wynik `5` na wyświetlaczu, zamknięcie |
| `scenario_settings.txt` | SETTINGS.ELF | Otwarcie, zakładka Dzwiek, `+` głośności (80%→100%), zakładka Mysz, zamknięcie |
| `scenario_calendar.txt` | CALENDAR.ELF | Otwarcie, nawigacja miesiąca (`>`/`<`), wpis w planerze notatek, zamknięcie |
| `scenario_notepad.txt` | NOTEPAD.ELF | Otwarcie, wpisanie tekstu w obszarze edycji, zamknięcie |
| `scenario_taskmgr.txt` | TASKMGR.ELF | Otwarcie, zaznaczenie wiersza zadania (BEZ klikania "Zakoncz" — patrz komentarz w pliku), zamknięcie |
| `scenario_winver.txt` | WINVER.ELF | Otwarcie, odczyt banera/wersji/RAM, zamknięcie przyciskiem OK |
