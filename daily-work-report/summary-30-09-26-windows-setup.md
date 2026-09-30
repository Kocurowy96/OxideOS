# Raport: setup_windows.bat + naprawa prawdziwej przyczyny "skrypty nie działają po clone"

**Data:** 30-09-2026
**Gałąź:** main-0k7z9q
**Commit:** (patrz `git log` — commit tworzony razem z tym raportem)

## Zadanie
Prośba: dodać skrypt `.bat`, który automatycznie odpala okno WSL, robi `sudo apt install
<pakiety>` (użytkownik wpisuje hasło), a potem automatycznie klonuje repo — żeby nie było
sytuacji, że ktoś źle sklonuje repo i skrypty nie zadziałają.

## Co zrobiono

### 1. Znaleziona prawdziwa przyczyna problemu, którego dotyczyła prośba
Przy projektowaniu skryptu setupowego sprawdziłem co faktycznie potrzeba żeby świeży clone
zadziałał — i okazało się, że **`limine_dir/` i `iso_root/boot/limine/` (oba gitignored)
nigdy nie były budowane przez żaden skrypt w tym repo**. `scripts/make_disk.sh` zakładał że
`limine_dir` już ma gotowy `Makefile` (`make -C limine_dir`), co na świeżym clone jest
niemożliwe — ten katalog w ogóle nie istnieje. Potwierdzone wprost w historii: wiele
starszych `daily-work-report/*.md` (np. `summary-26-09-26-1512.md`,
`summary-25-09-26-0918.md`, `summary-24-09-26-0924.md`) zawiera zdanie w stylu
"`limine_dir`/`iso_root` trzeba było założyć ręcznie od zera" — czyli **to była ręczna
procedura wykonywana od nowa w każdej sesji chmurowej**, nigdzie niezapisana w kodzie, tylko
w notatkach. Dokładnie to ryzyko o którym mówiła prośba — tylko głębsze niż sam `git clone`.

### 2. Naprawa u źródła: `scripts/make_disk.sh`
Skrypt teraz sam odtwarza oba katalogi z **już commitowanego** `limine-12.5.2/` (pełne
źródło Limine, `configure` wygenerowany i zacommitowany — `./bootstrap`/autoconf
niepotrzebny):
- Gdy brak `limine_dir/limine`/`limine_dir/bin/limine`: odpala `./configure --prefix=...
  --enable-bios --enable-bios-cd --enable-uefi-x86-64 --enable-uefi-cd && make && make
  install` wewnątrz `limine-12.5.2/` (flagi `--enable-*` konieczne — `configure` domyślnie
  nie włącza żadnego portu bootloadera, udokumentowane w `limine-12.5.2/INSTALL.md`).
- Kopiuje zbudowane binaria bootloadera (`limine-bios-cd.bin`, `limine-bios.sys`,
  `limine-uefi-cd.bin`, `BOOTX64.EFI` — instalują się do `limine_dir/share/limine/` w
  layoucie GNU `make install`, z fallbackiem na starszy układ `limine_dir/bin/` dla
  zgodności wstecznej) do `iso_root/boot/limine/`/`iso_root/EFI/BOOT/`.
- Zapisuje statyczny, niezmienny `limine.conf` (ten sam od początku projektu, wyciągnięty z
  wcześniej ręcznie składanej wersji).

### 3. `scripts/setup_windows.bat` — oryginalna prośba
Sprawdza obecność WSL i przynajmniej jednej zainstalowanej dystrybucji, instaluje pełną
listę paczek przez `apt` (`git build-essential cmake xorriso qemu-system-x86 imagemagick
e2fsprogs nasm mtools gdb python3` — `nasm`/`mtools` dopisane, bo okazały się realnie
potrzebne do zbudowania Limine, wcześniej pominięte na liście requirements), użytkownik
wpisuje hasło sudo interaktywnie w tym samym oknie. Potem pyta o docelowy folder (domyślnie
obok samego skryptu) i klonuje repo (`git clone https://github.com/Kocurowy96/OxideOS`) —
albo robi `git pull` jeśli pod tą ścieżką już jest gotowy checkout, zamiast nadpisywać.
Odmawia nadpisania istniejącego, niepustego folderu który NIE jest repo Gita.

### 4. `README.md`
- "Requirements" (sekcja Linux) zaktualizowane o `git`/`nasm`/`mtools` i notkę że pierwsze
  uruchomienie samo bootstrapuje Limine.
- Sekcja "Running on Windows": krok 2 zamieniony na `scripts\setup_windows.bat` jako
  zalecany pierwszy krok, z jawnym wyjaśnieniem *po co* (żeby setup nie mógł pójść "w pół
  dobrze" — zły folder, brakujący pakiet).

## Weryfikacja
**Najbardziej rygorystyczny test jaki dało się przeprowadzić w tym środowisku**: usunięte
`limine_dir/`, `iso_root/` i wszystkie artefakty `configure`/`make` wewnątrz
`limine-12.5.2/` — czyli symulacja dokładnie tego stanu w jakim jest repo zaraz po
`git clone` na czystej maszynie. Potem `scripts/test_headless.sh 25` **od zera, bez
żadnej ręcznej ingerencji**: **PASS** — pełny bootstrap Limine (configure+make+install),
złożenie `iso_root/boot/limine/`, budowa `disk.img`+ISO, boot do `Ext2: Initialized
successfully`, DHCP bind, ARP prewarm, brak panic. Drugie uruchomienie (limine_dir już
istnieje) poprawnie **pomija** kosztowny bootstrap Limine i kończy się w ~22s zamiast
przechodzić przez `configure`/`make` ponownie — potwierdza że gating na istnienie
`limine_dir/limine` działa idempotentnie.

`git status` po całości: brak przypadkowo dodanych artefaktów budowania (`limine_dir`,
`iso_root`, `disk.img`, `*.iso` — wszystkie poprawnie ignorowane przez `.gitignore`).

**`scripts/setup_windows.bat` samo w sobie nieprzetestowane na prawdziwym Windowsie** (ten
sam, już wcześniej odnotowany limit tego środowiska — kontener linuksowy, brak WSL/`cmd.exe`)
— ale to jedyna część tej zmiany której nie dało się tu zweryfikować bezpośrednio.
Naprawka `make_disk.sh` jest w 100% zweryfikowana (zwykły bash, testowalny wprost tutaj).

## Napotkane problemy / obserwacje
Największe zaskoczenie tej sesji: problem o który pytał Wiktor ("żeby ktoś nie sklonował
repo źle i skrypty nie zadziałały") istniał już od dawna, tylko w innym miejscu niż
sugerowałoby pytanie — nie w samym `git clone`, tylko w kompletnie niezeskryptowanym kroku
Limine, ukrytym w gitignore i odtwarzanym ręcznie co sesję (widoczne tylko po przejrzeniu
historii `daily-work-report/`). Naprawa tego jest dużo ważniejsza dla realnego celu
("świeży clone + skrypty = działa") niż sam `setup_windows.bat` — ten drugi jest wygodą,
tamto było twardym blockerem.

## Co zostało / kolejne kroki
`setup_windows.bat` czeka na potwierdzenie na prawdziwym Windowsie. Poza tym — zakres
zrealizowany w całości, nic zawieszonego w połowie.

## Push
Zmiany (`scripts/make_disk.sh`, `scripts/setup_windows.bat`, `README.md`,
`CoworkWithClaude/TASKS.md`, ten raport) do wypchnięcia na `origin/main-0k7z9q` — ten sam
branch co otwarty już PR #1, więc PR zaktualizuje się automatycznie bez otwierania nowego.
