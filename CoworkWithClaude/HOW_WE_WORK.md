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
