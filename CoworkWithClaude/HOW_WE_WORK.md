# Jak pracujemy nad OxideOS

Spisane ustalenia i nawyki — żeby były w repo (widoczne dla każdej sesji Claude, w tym
agenta w chmurze na `daily-work`), nie tylko w czyjejś pamięci.

## Podział ról

- **Kod** (jądro, sterowniki, GUI, syscalle, aplikacje) — Claude.
- **Assety** (tapety, ikony, dźwięki, grafiki) i testowanie w QEMU — Kocurowy96.

## Rytm pracy

- **Dni szkolne (pon-pt)** — agent w chmurze bierze pierwszą pozycję z `TASKS.md` → "Do zrobienia
  teraz", robi jeden sensowny kawałek, testuje, pushuje na gałąź `daily-work` (nigdy `main`).
  Szczegóły mechanizmu: `TASKS.md`.
- **Wieczory / weekendy** — wspólna sesja: testujemy `daily-work` w prawdziwym QEMU z GUI,
  decydujemy czy mergować do `main`, rozbudowujemy `TASKS.md` o nowe pomysły/poprawki.

## Commity i push

- **Nie commitować po każdej drobnej zmianie.** Commit robimy przy dużych/milowych zmianach albo
  gdy dodajemy coś zaawansowanego "w biegu" (np. sieć, port Ladybirda, przepisanie na ext4).
  Mniejsze kroki mogą leżeć niescommitowane, aż uzbiera się sensowna całość.
- Po commicie na `main` — push od razu (repo jest publiczne, ale to solo-projekt, nie ma ryzyka
  kolizji z czyjąś równoległą pracą).
- Na `daily-work` — agent w chmurze commituje i pushuje sam, ale **nigdy nie dotyka `main`**.

## Testowanie

- **Po każdej sensownej zmianie Claude po prostu odpala `./scripts/run.sh`** (GUI, w tle) —
  bez pytania o zgodę za każdym razem. Kocurowy96 testuje interaktywnie i zgłasza co widzi.
- Do szybkiej, bezinteraktywnej weryfikacji (czy coś się kompiluje/nie zawiesza) — headless boot
  w QEMU (`-display none`, log z serial, timeout) zamiast czekania na człowieka przy GUI. Ten
  wzorzec był używany ręcznie 2026-09-11; docelowo ma się rozbić na `scripts/test_headless.sh`
  (patrz `TASKS.md`).
- Zanim coś zgłosimy jako "działa" — zawsze pełny, czysty rebuild (`rm -rf build && cmake -B
  build -S . && cmake --build build`), nie poleganie na przyrostowym.
- Przy trudnych do zreprodukowania bugach: nie zgadywać w ciemno. Odtworzyć syntetycznie
  (tymczasowy kod diagnostyczny + headless run), potwierdzić przyczynę na twardych danych,
  dopiero potem naprawiać. Usunąć kod diagnostyczny po potwierdzeniu.
- **Port debugowy (od 2026-09-20)** — `scripts/screendump.sh [out.png] [wait_s]` robi
  zrzut ekranu GUI z headless QEMU przez QMP `screendump`, bez żadnego wyświetlacza. Nie
  jest ograniczony do maszyny Kocurowy96 — to samo QMP działa identycznie w kontenerze
  agenta w chmurze, więc **agent też może wizualnie zweryfikować zmianę w GUI**, nie tylko
  czytać log serialowy. `scripts/headless_interact.sh <scenariusz.txt> [katalog_wyjściowy]
  [boot_wait_s]` dokłada sterowanie mysza/klawiatura (klik, scroll, wpisywanie tekstu) +
  zrzuty ekranu w trakcie, z pliku-scenariusza (format opisany w komentarzu na górze
  skryptu) — pozwala np. otworzyć Menu Start, kliknąć apkę, zrobić zrzut, bez człowieka
  przy klawiaturze. **Ważne odkrycie przy wdrażaniu:** klik trzeba trzymać chwilę (down →
  sleep ~150ms → up) — `mouse_clicked = mouse_left && !prev_mouse_left` w
  `compositor.cpp` próbkuje stan raz na przebieg pętli renderowania, a klik bez
  przytrzymania (down+up niemal jednocześnie) często nie trafiał w to okno próbkowania i
  GUI go "nie widziało". `qmp_input.py` ma to już wbudowane (`CLICK_HOLD_SECONDS`), nie
  trzeba tego pamiętać przy każdym użyciu. `scripts/gdb_inspect.sh <plik_komend_gdb>
  [wait_s]` dokłada etap 2 — podgląd rejestrów/pamięci/aktualnie wykonywanej instrukcji
  przez GDB stub QEMU (`-s`, bez `-S` — łapie stan "w locie", nie od resetu), przydatne przy
  trudnych bugach (np. coś jak scheduler 2026-09-11) zamiast zgadywać z samego logu
  serialowego.

## Styl kodu

- Ten kod ma swoje konwencje wypracowane wcześniej (część pisana z pomocą innych narzędzi AI) —
  trzymamy się ich, nie "ulepszamy" bez potrzeby przy okazji niepowiązanej zmiany.
- Przykład: FAT32 ma celowo powtórzoną 3x pętlę skanowania katalogu (w `FindDirectoryCluster`,
  `ReadFile`, `WriteFile`) zamiast wspólnej abstrakcji — tak jest w całym projekcie, minimalne
  zmiany > przedwczesna abstrakcja.
- Komentarze tylko tam, gdzie CO nie jest oczywiste z kodu, tylko DLACZEGO (nieoczywiste
  ograniczenie, obejście konkretnego buga, coś co zaskoczyłoby czytającego).
- Kernel: bez wyjątków C++, bez RTTI, freestanding. Aplikacje userspace: bardzo minimalny
  libc/libgui, każda apka definiuje swoje małe helpery (itoa/strcpy/strcat) lokalnie zamiast
  współdzielonej biblioteki standardowej.

## Uwaga dla agenta w chmurze (daily-work)

Nikt nie jest obecny, żeby zatwierdzić monity o pozwolenie w trakcie autonomicznej sesji.
Sprawdzone empirycznie (2026-09-11, testowe uruchomienie): `rm -rf` na katalogu (nawet
stworzonym przez samego agenta, np. `limine_dir/`) potrafi wywołać monit "Dangerous rm
operation", na który nikt nie odpowie — sesja wisi aż do timeoutu. Agent ma w swoim
promptcie instrukcję żeby unikać takich wzorców i wybierać bardziej celowane alternatywy
(np. `find dir -mindepth 1 -delete` zamiast `rm -rf dir/*`), ale warto o tym pamiętać przy
rozbudowie instrukcji na przyszłość.

## Znane, zaakceptowane długi techniczne

Nie naprawiać "przy okazji" bez wyraźnej decyzji — to świadome uproszczenia, nie przeoczenia:

- **Brak tabel stron per-proces** — jedna wspólna przestrzeń adresowa dla wszystkich tasków.
  Per-task kernel stack już naprawiony (2026-09-11), ale to kolejny krok w tym samym kierunku.
- **Brak sieci** — zero stosu TCP/IP w OxideOS.
- **FAT32 zamiast ext4** — docelowo ext4 (nie exFAT), ale to osobna, duża sesja.
- **`critical.h`** (`EnterCritical`/`ExitCritical`) to prowizoryczny, zagnieżdżalny cli/sti,
  nie prawdziwy lock — wystarcza na jeden rdzeń, do zastąpienia przy większym przepisywaniu.

Pełna, aktualna lista tego co zrobione/do zrobienia/do przegadania: `TASKS.md`.
