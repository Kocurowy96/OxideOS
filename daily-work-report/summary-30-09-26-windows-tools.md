# Raport: narzędzia `.bat` dla Windowsa + sekcja README

**Data:** 30-09-2026
**Gałąź:** main-0k7z9q
**Commit:** (patrz `git log` — commit tworzony razem z tym raportem)

## Zadanie
Prośba Wiktora: zrobić windowsowe odpowiedniki wszystkich skryptów z `scripts/` (`.bat`),
zaktualizować `README.md` o to co się na Windowsie otwiera/jak to uruchomić, dopisać
dokumencik — i tym razem, bo Wiktor jest poza komputerem, zamiast tylko pushować na
`main-0k7z9q` **otworzyć pull request do `main`** (merge zrobi sam z aplikacji mobilnej).

## Co zrobiono
**Siedem plików `scripts/*.bat`** — po jednym na każdy istniejący `.sh`: `build.bat`,
`make_disk.bat`, `run.bat`, `test_headless.bat`, `screendump.bat`, `headless_interact.bat`,
`gdb_inspect.bat`.

**Decyzja projektowa**: toolchain budowania OxideOS (`mke2fs`/`debugfs` do obrazu ext2,
`xorriso` do ISO, `gcc`/`ld` produkujące freestanding ELF bez PIE) jest czysto
linuksowy — nie ma naturalnego odpowiednika na gołym Windowsie (standardowy MinGW/MSYS2
`gcc` celuje w Windows PE, nie w ELF którego oczekuje własny loader OxideOS). Zamiast
pisać "natywną" reimplementację w `.bat` (podwójne utrzymanie dwóch wersji tej samej
logiki, łatwe do rozjechania), każdy `.bat` to **cienki wrapper odpalający prawdziwy `.sh`
wewnątrz WSL2**: `wsl bash -lc "./scripts/<nazwa>.sh %*"`, z argumentami przekazywanymi
dalej bez zmian. Każdy wrapper najpierw sprawdza `where wsl` i wypisuje czytelny błąd z
linkiem do instalacji WSL2, jeśli go brak, zamiast ciche się wywalić.

**`README.md`** — nowa sekcja "Running on Windows" (po angielsku, zgodnie z resztą pliku,
który jest już po angielsku od wcześniejszego commitu `a8db92c`): instrukcja setupu WSL2
+ instalacji paczek z listy "Requirements" wewnątrz dystrybucji, `scripts\run.bat` jako
punkt wejścia, jasno opisane co się otwiera (okno QEMU przez WSLg na pulpicie Windows,
dokładnie jak przy `./scripts/run.sh` na Linuksie) i co nie otwiera nic na ekranie
(`test_headless.bat`/`screendump.bat`, bo ich `.sh` odpowiedniki używają `-display none`).
Uczciwie zaznaczone, że ścieżki dla `headless_interact.bat`/`gdb_inspect.bat` trzeba podać
tak jak widzi je WSL (nie ma translacji ścieżek windowsowych→WSL w wrapperze).

**`CoworkWithClaude/TASKS.md`** — nowa pozycja w "Zrobione" opisująca tę zmianę.

## Weryfikacja
Same `.sh` skrypty są niezmienione — `scripts/test_headless.sh 25` uruchomiony ponownie
dla pewności: **PASS**, pełny boot do `Ext2: Initialized successfully`, DHCP
DISCOVER→OFFER→REQUEST→bound, ARP prewarm, brak panic, brak regresji.

**Uczciwie odnotowane, nie ukryte**: same pliki `.bat` **nie zostały przetestowane na
prawdziwym Windowsie** — to środowisko to kontener linuksowy bez dostępu do `cmd.exe`/WSL,
więc nie da się tu fizycznie odpalić `.bat`-a ani potwierdzić że WSLg faktycznie forwarduje
okno QEMU tak jak opisano. Zawartość została starannie przemyślana (ten sam wzorzec
`wsl bash -lc "..."` w każdym pliku, sprawdzenie obecności `wsl` przed próbą użycia,
przekazanie `%*` dla skryptów przyjmujących argumenty) i README wprost mówi że to
nieprzetestowane na realnym Windowsie — zamiast zmyślać wynik testu którego nie da się
tutaj wykonać.

## Napotkane problemy / obserwacje
Brak niespodzianek przy samej pracy. Jedyna realna decyzja do podjęcia była
architektoniczna (WSL-wrapper vs. próba natywnej reimplementacji) — rozstrzygnięta na
korzyść WSL-wrappera, bo jedyna alternatywa (natywny cross-compiler + ręczna
reimplementacja `mke2fs`/`debugfs`/`xorriso` w batchu) byłaby w praktyce drugim,
równoległym, trudnym do utrzymania budowaniem tego samego projektu.

## Co zostało / kolejne kroki
Realne potwierdzenie działania na prawdziwym Windowsie — zostaje do zrobienia przez kogoś
z fizycznym dostępem do maszyny z Windows + WSL2 (informacja w README już to zaznacza jako
otwarty punkt, nie ukryte założenie).

## Push
Zmiany (`scripts/*.bat` ×7, `README.md`, `CoworkWithClaude/TASKS.md`, ten raport) pushnięte
na `origin/main-0k7z9q`. **Tym razem, na wyraźną prośbę Wiktora (jest poza komputerem),
otwarty też pull request `main-0k7z9q` → `main`** — merge zrobi sam z aplikacji mobilnej
GitHuba, zamiast zwykłego "zostaje na branchu do ręcznego mergowania" używanego przez resztę
tej sesji.
