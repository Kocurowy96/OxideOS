# Plan: skrypty testowe i "port debugowy" (do przejrzenia razem, jeszcze niezaimplementowane)

Przygotowane 2026-09-11 wieczorem, do przedyskutowania i wdrożenia na kolejnej wspólnej sesji.
Nic z tego nie jest jeszcze w repo — to szkic do akceptacji/poprawek.

---

## Część 1: rozbicie `scripts/run.sh`

### Dlaczego
Dziś ręcznie sklejałem headless-test za każdym razem z fragmentów `run.sh` w scratchpadzie —
działało, ale było powtarzalne i podatne na literówki (np. różne wersje między testami).
Jeden zestaw skryptów = ten sam kod dla mnie, dla agenta w chmurze i dla `test_headless.sh`.

### Proponowany podział

**`scripts/build.sh`** — sam kernel + apki, bez dysku/ISO/QEMU
```bash
#!/bin/bash
set -e
cd "$(dirname "$0")/.."

cmake -B build
cmake --build build

mkdir -p apps/settings  # istniejący workaround na brak katalogu przy pierwszym uruchomieniu
gcc -c apps/libgui/gui.c -o apps/libgui/gui.o -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector -mno-sse -mno-sse2 -mno-mmx -msoft-float
ar rcs apps/libgui/libgui.a apps/libgui/gui.o

for app in hello settings calculator paint calendar winver clock notepad taskmgr; do
    gcc -c "apps/$app/main.c" -o "apps/$app/main.o" -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector -mno-sse -mno-sse2 -mno-mmx -msoft-float -I apps/libgui
done
# uwaga: adresy -Ttext per-apka (0x400000, 0x500000, ...) trzeba zachować jak w oryginalnym
# run.sh - tu do dopracowania, żeby nie duplikować literałów w dwóch miejscach (może osobna
# tablica app->adres w tym skrypcie?)
```
*Do ustalenia razem:* czy pętla po aplikacjach z tablicą adresów jest OK, czy wolisz zostawić
jawne wywołania per-apka (czytelniejsze przy debugowaniu, ale bardziej repetytywne) — tak jak
jest teraz w `run.sh`. Trzymając się zasady "nie ulepszać bez potrzeby" być może lepiej zostawić
jawne linijki i tylko wydzielić je do osobnego pliku, bez zmiany stylu.

**`scripts/make_disk.sh`** — dysk.img + ISO z już zbudowanych binarek
- Cała logika `mformat`/`mmd`/`mcopy`/konwersji PNG→BMP z `assets/`/`xorriso`/`limine bios-install`
  z obecnego `run.sh`, bez zmian w logice — czysto przeniesione.
- Zależy od `scripts/build.sh` (musi być już zbudowane `build/kernel.elf` + apki).

**`scripts/test_headless.sh`** — build + dysk + boot w QEMU bez GUI
```bash
#!/bin/bash
set -e
cd "$(dirname "$0")/.."

./scripts/build.sh
./scripts/make_disk.sh

QEMU_FLAGS="-m 512M -cdrom oxideos.iso -hda disk.img -boot d -serial stdio -display none"
if [ -e /dev/kvm ]; then
    QEMU_FLAGS="-enable-kvm $QEMU_FLAGS"
fi

LOG=$(mktemp)
timeout "${1:-15}" qemu-system-x86_64 $QEMU_FLAGS > "$LOG" 2>&1 || true

echo "--- LOG ($LOG) ---"
cat "$LOG"

# Bardzo prosty PASS/FAIL - do dopracowania jakich linii dokladnie szukac
if grep -q "FAT32: Initialized successfully" "$LOG" && ! grep -qi "kernel panic" "$LOG"; then
    echo "PASS: boot dotarl do FAT32 init, brak panic"
    exit 0
else
    echo "FAIL: albo brak FAT32 init, albo wykryto panic"
    exit 1
fi
```
*Do ustalenia razem:* jakie dokładnie kryteria PASS/FAIL mają sens (dziś sprawdzałem to
ręcznie czytając log - "FAT32: Initialized successfully" + brak "Kernel Panic" to minimum,
ale może warto dorzucić np. sprawdzenie że dotarło do "GUI and Multitasking initialized").
Też: czy `timeout` ma być parametrem (domyślnie 15s), czy stałą wartością.

**`scripts/run.sh`** — zostaje jako punkt wejścia z GUI, ale:
```bash
#!/bin/bash
set -e
cd "$(dirname "$0")/.."
./scripts/build.sh
./scripts/make_disk.sh
qemu-system-x86_64 -enable-kvm -m 512M -cdrom oxideos.iso -hda disk.img -boot d -serial stdio -audiodev pa,id=snd0 -device AC97,audiodev=snd0
```
Zachowuje dokładnie to samo zachowanie co dziś (audio, KVM), tylko woła wspólne skrypty
zamiast duplikować kod.

---

## Część 2: "port debugowy" — wrappery na możliwości QEMU

Robić **etapami**, każdy kolejny tylko jeśli poprzedni się sprawdzi:

### Etap 1: zrzuty ekranu (najprostszy, zrobić najpierw) — ZROBIONE (2026-09-20)
`scripts/screendump.sh [out.png] [wait_s]` + `scripts/qmp_screendump.py` (klient QMP w
Pythonie, handshake capabilities + `screendump`). Boot headless z `-qmp unix:...,server,nowait`,
czeka `wait_s` (domyślnie 8s), zrzuca PPM, konwertuje do PNG (`magick`/`convert`), drukuje
log serialowy, sprząta gniazdo/proces QEMU. Przetestowane end-to-end — działa.

- QMP (`-qmp unix:/tmp/oxideos-qmp.sock,server,nowait`) + prosty Python/bash wrapper wołający
  `screendump` przez gniazdo. Zapisuje PPM, konwertuje do PNG (mamy już ImageMagick w projekcie
  do konwersji assetów, więc `magick shot.ppm shot.png` za darmo).
- Użycie: szybkie "jak wygląda GUI teraz" bez czekania na człowieka przy ekranie — przydatne
  też dla mnie żeby zweryfikować UI zamiast tylko czytać logi serial.

### Etap 2: inspekcja RAM/rejestrów/instrukcji (GDB stub QEMU) — ZROBIONE (2026-09-20)
`scripts/gdb_inspect.sh <plik_komend_gdb> [wait_s]` - boot headless z `-s` (gdbstub na
`:1234`, celowo BEZ `-S` - gość bootuje normalnie, GDB dołącza się po `wait_s` i przerywa
wykonanie w tym momencie, nie od resetu, żeby móc łapać stan "w locie"), potem
`gdb -batch -ex "target remote :1234" -ex <komenda z pliku>...` - jedna komenda GDB na
linię w pliku wejściowym. Celowo bez `-enable-kvm` - czysty TCG jest przewidywalny pod GDB,
KVM bywa ograniczony w zależności od wersji. Kernel.elf nie ma pełnego DWARF (kompilujemy
bez `-g`), ale ma symbole (mangled C++, GDB demanguje automatycznie) - `bt`/`x/i $pc`
pokazują realne nazwy funkcji. Przetestowane na żywo: złapało system w środku
`Framebuffer::Clear` wywołanego z `Compositor::Render` z `DesktopTask` - dokładnie taki
wgląd, jaki by się przydał przy diagnozowaniu buga ze schedulerem 2026-09-11.

### Etap 3: interakcja mysz/klawiatura bez człowieka (QMP `input-send-event`) — ZROBIONE (2026-09-20)
`scripts/qmp_input.py` (klient QMP, dzieli `qmp_client.py` z `qmp_screendump.py`) - komendy
`move X Y`, `click --button left/right/middle [--at X Y]`, `scroll up/down [--amount N]`,
`key QCODE`, `type "tekst"`. `scripts/headless_interact.sh <scenariusz> [katalog] [boot_wait_s]`
spina to z etapem 1 (zrzuty ekranu) w jeden przebieg sterowany plikiem-scenariuszem (format w
komentarzu na górze skryptu) - boot headless, seria akcji (ruch/klik/scroll/klawiatura/zrzut),
sprzątnięcie QEMU na końcu.

**Namierzony i naprawiony realny problem przy testowaniu:** pierwsza wersja `click()` robiła
`btn down` i `btn up` bez przerwy - kliknięcia w ogóle nie były wykrywane przez GUI (testowane
na przycisku "OK" okienka powitalnego i na "Start" - kursor trafiał dokładnie w cel, ale nic
się nie działo). Przyczyna: `compositor.cpp` wykrywa klik przez `mouse_left && !prev_mouse_left`,
próbkowane raz na przebieg pętli renderowania - down+up bez przerwy potrafiło "zmieścić się"
między dwoma próbkowaniami i zostać całkowicie przeoczone. Fix: 150ms przytrzymania między
down i up (`CLICK_HOLD_SECONDS` w `qmp_input.py`). Po tej poprawce potwierdzone działające:
zamknięcie okienka powitalnego kliknięciem OK, otwarcie Menu Start kliknięciem, uruchomienie
apki z Menu Start, wpisywanie tekstu klawiaturą (log serialowy pokazuje wpisane znaki), scroll
(nie wywala się - w OxideOS nie ma dziś żadnego scrollowalnego widgetu do wizualnej weryfikacji,
ale mechanizm identyczny jak klik).

**Zastosowanie dla agenta w chmurze:** to samo QMP działa identycznie bez żadnego wyświetlacza,
więc `headless_interact.sh`/`screendump.sh` mogą być używane też w kontenerze agenta - daje
mu to realną weryfikację wizualną zmian w GUI, nie tylko czytanie logu serialowego. Patrz
`HOW_WE_WORK.md`.

### Etap 4: nagrywanie wideo (opcjonalne, najmniej pilne)
- **Tylko lokalnie** (Garuda Linux/KWin/Wayland, ASUS TUF Gaming A15) - w chmurze nie ma sensu.
- ffmpeg przechwytujący z sesji Wayland, albo złożenie serii `screendump` w wideo.

---

## Otwarte pytania do rozmowy

1. Czy pętla po aplikacjach w `build.sh` (tablica nazwa→adres) jest OK, czy wolisz jawne
   linijki jak dziś w `run.sh`?
2. Jakie dokładnie kryteria PASS/FAIL dla `test_headless.sh`?
3. Czy zaczynamy od razu od etapu 1 portu debugowego (zrzuty ekranu), czy najpierw
   dokańczamy same skrypty testowe?

## Ustalenia z testowego uruchomienia agenta (2026-09-11 wieczorem, przed weekendem)

Zrobiliśmy realny testrun automatyzacji pon-pt (zamiast czekać do poniedziałku w niepewności):

- **Świeże środowisko w chmurze nie ma zainstalowanego `qemu-system-x86_64`/`xorriso`/`mtools`/
  `nasm`** — trzeba je doinstalować przez `apt-get` przy każdym uruchomieniu (środowisko
  najwyraźniej nie trzyma tego między sesjami). `test_headless.sh` powinien to uwzględniać:
  spróbować `apt-get install`, a jeśli się nie uda (np. brak uprawnień/sieci), nie failować
  całego skryptu — tylko jasno zgłosić że pełna weryfikacja boot-testem nie była możliwa i
  polegać na samym czystym kompilowaniu.
- **Ważne odkrycie:** `rm -rf` na katalogu — nawet takim, który sam skrypt/agent stworzył
  chwilę wcześniej (np. `limine_dir/` przy próbie "wyczyszczenia i zbudowania od nowa") —
  potrafi wywołać interaktywny monit bezpieczeństwa. W sesji bez człowieka przy klawiaturze
  to zawiesza cały test do timeoutu. **`test_headless.sh` (i każdy inny skrypt, który może
  kiedyś działać bez nadzoru) powinien unikać `rm -rf` na katalogach — woleć np.
  `find dir -mindepth 1 -delete`, albo po prostu nie czyścić `limine_dir/` wcale (configure/make
  Limine'a radzą sobie z ponownym uruchomieniem bez czyszczenia).** `rm -rf build/` przy
  właściwym rebuildzie kernela jest OK (to jawnie ustalony, bezpieczny wzorzec — build/ to
  czysto wygenerowany katalog wyjściowy).
- `limine_dir/` trzeba budować od zera (`configure --enable-bios-cd --enable-bios
  --enable-uefi-cd --enable-uefi-x86-64 && make`), bo jest w `.gitignore` — świeży klon go
  nie ma. `test_headless.sh` powinien to obsłużyć (build jeśli nie istnieje, nie czyścić jeśli
  istnieje).
