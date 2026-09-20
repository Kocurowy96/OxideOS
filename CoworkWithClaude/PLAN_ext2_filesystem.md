# Plan: przejście FAT32 → ext2 (podzbiór, bez dziennika/extentów)

Ustalone 2026-09-20 wieczorem. Powód: Kocurowy96 zaczyna mieć mniej czasu (sprawdziany,
lektury), więc zamiast próbować zrobić to jednego wieczoru, rozbijamy na wąskie kawałki,
które codzienny agent w chmurze może po kolei robić w tygodniu (`CoworkWithClaude/TASKS.md`).

## Dlaczego ext2, nie pełne ext4

Pełne ext4 (drzewo extentów zamiast prostych wskaźników bloków, dziennik jbd2, sumy
kontrolne metadanych, opcjonalnie htree dla dużych katalogów) to osobny, wielotygodniowy
projekt — większy niż cały OxideOS do tej pory. Podzbiór w stylu **ext2** (proste
direct/indirect wskaźniki bloków, bez dziennika) jest realnie osiągalny w rozsądnym czasie,
a mimo to zyskujemy realną przewagę nad FAT32: prawdziwy Unix-owy format, który da się
zamontować i zdebugować zwykłym `mount -t ext2` / `debugfs` na Linuksie zamiast tylko
specjalnych narzędzi (`mtools`). Extenty/dziennik/reszta ext4-specific można dołożyć
później, jeśli kiedyś faktycznie będzie potrzebna — nie zakładamy, że tak będzie.

## Kluczowe odkrycie: to NIE dotyka łańcucha bootowania

Limine (bundlowana wersja w `limine-12.5.2/`) ma wbudowane sterowniki tylko dla **FAT32 i
ISO9660** (`limine-12.5.2/common/fs/`) — nigdy nie miał i nie ma wsparcia dla ext2/ext4.
To by wyglądało na blokadę, ale sprawdziliśmy: Limine **nigdy nie dotyka `disk.img`**.
Boot wygląda tak:
- `oxideos.iso` (ISO9660, `-cdrom`, `-boot d`) — to jedyna rzecz którą czyta Limine, żeby
  znaleźć `boot/kernel.elf` i `limine.conf`. Zostaje bez zmian.
- `disk.img` (ATA/hda, budowany dziś przez `mtools` w `scripts/make_disk.sh`) — to czysto
  nasza przestrzeń, czytana wyłącznie przez własny sterownik w kernelu (dziś `FAT32::`,
  wywoływany przez `VFS::`) dla `/usr/bin`, assetów, plików użytkownika.

**Wniosek:** przejście na ext2 dotyczy WYŁĄCZNIE `disk.img` i kodu w `kernel/fs/`. Zero
zmian w Limine, `iso_root/`, `oxideos.iso`. Nawet jeśli sterownik ext2 miałby buga —
system i tak wystartuje (Limine załaduje kernel.elf normalnie), tylko VFS się nie zamontuje.
Dużo mniejsze ryzyko niż "przepisanie systemu plików" brzmi na pierwszy rzut oka.

## Narzędzia hosta

`mke2fs`, `debugfs`, `e2fsck`, `mkfs.ext2` — potwierdzone dostępne lokalnie (pakiet
`e2fsprogs`, prawdopodobnie trzeba będzie doinstalować w kontenerze agenta w chmurze, tak
jak dziś `qemu`/`xorriso`/`mtools`, środowisko nie trzyma pakietów między sesjami).
`debugfs -w -R "write <plik_hosta> <plik_w_obrazie>" obraz.img` to odpowiednik `mcopy` z
`mtools` — wstrzykuje pliki do obrazu ext2 bez montowania, bez roota.

## Kształt sterownika

Nowy `kernel/fs/ext2.h`/`ext2.cpp`, interfejs 1:1 jak dzisiejszy `kernel/fs/fat32.h`, żeby
`kernel/fs/vfs.cpp` prawie się nie zmienił (woła `Ext2::` zamiast `FAT32::`):
```cpp
class Ext2 {
public:
    static void Init();
    static bool ReadFile(const char* path, uint8_t** out_buffer, uint32_t* out_size);
    static bool WriteFile(const char* path, const uint8_t* buffer, uint32_t size);
    static void FreeFile(uint8_t* buffer, uint32_t size);
    static int ListDirectory(const char* path, DirEntry* out_entries, int max_entries);
};
```
`kernel/fs/fat32.cpp` zostaje w drzewie nietknięty przez cały czas trwania prac (do
ewentualnego usunięcia dopiero po pełnej weryfikacji ext2 w Fazie 3) — zero ryzyka
regresji w międzyczasie, bo VFS dalej woła FAT32 aż do świadomego przełączenia.

## Zakres funkcji ext2 na start (świadomie pominięte na razie)

- Bloki: proste wskaźniki (12 bezpośrednich + pojedynczo pośredni na start; podwójnie/
  potrójnie pośredni dopiero jeśli jakiś plik faktycznie tego wymaga — dziś największe
  pliki w OxideOS to bitmapy/WAV, prawdopodobnie mieszczą się dużo wcześniej).
- Katalogi: klasyczny liniowy format wpisów ext2 (bez htree — nie potrzebujemy indeksu przy
  garstce plików w `/usr/bin`).
- Bez uprawnień/właściciela — jednoużytkownikowy hobby-OS, ignorujemy `uid`/`gid`/`mode`
  poza tym co potrzebne żeby `mke2fs`-owy obraz w ogóle się parsował.
- Bez dziennika (to ext2, nie ext3/ext4) — `critical.h`-owy "duct tape" lock zostaje,
  ale przy okazji dobra okazja żeby przynajmniej skonsolidować go w jeden spójny,
  udokumentowany lock wokół całego wejścia do sterownika (patrz Faza 0 niżej) zamiast
  rozrzuconych wywołań jak dziś w FAT32.

## Fazy (każda = potencjalnie jedna-dwie sesje agenta w tygodniu)

**Faza 0 — ten dokument + szkielet.** Zrobione 2026-09-20: decyzje architektoniczne
(ten plik), potwierdzone narzędzia hosta, potwierdzone że boot jest bezpieczny.

**Faza 1 — odczyt (największy pojedynczy kawałek, dalej dzielony w TASKS.md):**
1a. Parsowanie superbloku + tablicy deskryptorów grup bloków, log w stylu
    `Ext2: Initialized successfully.` (analogicznie do dzisiejszego FAT32).
1b. Odczyt tablicy i-węzłów (dany numer i-węzła → struktura i-węzła).
1c. Rozwiązywanie ścieżek (root i-węzeł → przejście po wpisach katalogowych po nazwie,
    np. `/usr/bin/HELLO.ELF`).
1d. `ListDirectory` — wylistowanie wpisów danego katalogu.
1e. Odczyt zawartości pliku przez bezpośrednie bloki (pliki ≤ 12×block_size).
1f. Odczyt przez pojedynczo pośrednie bloki (pliki większe).

**Faza 2 — zapis:**
2a. Bitmapa wolnych bloków (alokacja/zwalnianie).
2b. Bitmapa wolnych i-węzłów (alokacja/zwalnianie).
2c. `WriteFile` dla nowego pliku (nowy i-węzeł, zapis bloków, nowy wpis katalogowy).
2d. Powiększanie/skracanie istniejącego pliku (aktualizacja wskaźników bloków i rozmiaru
    w i-węźle) — potrzebne np. dla nadpisania `/notatka.txt` z Notatnika.

**Faza 3 — integracja i przełączenie:**
3a. `scripts/make_disk.sh`: `disk.img` budowany przez `mke2fs`+`debugfs` zamiast
    `mtools`/FAT32.
3b. `kernel/fs/vfs.cpp`: przełączenie z `FAT32::` na `Ext2::`.
3c. Pełny regres: każda apka, zapis/odczyt tapety w Ustawieniach, odtwarzanie WAV,
    zapis/odczyt w Notatniku (funkcja z dzisiejszego wieczoru) — powtórka scenariuszy z
    `headless_interact.sh` z dzisiejszej sesji jako gotowy zestaw testów regresyjnych.

**Faza 4 — later/opcjonalnie, nie planować teraz:** realne blokady per-struktura (dziś:
jeden globalny lock wystarczy), extenty/dziennik/ext4-specific — tylko jeśli kiedyś
faktycznie okaże się potrzebne.

## Jak to wejdzie do TASKS.md

Do "Do zrobienia teraz" trafia na start **tylko Faza 1a** (wąska, samodzielnie
weryfikowalna: sterownik nic jeszcze nie integruje z VFS, więc zero ryzyka dla
działającego systemu). Kolejne podpunkty (1b, 1c, ...) dopisywane do "Do zrobienia teraz"
sukcesywnie, w miarę jak poprzednie się kończą — nie wrzucać całej Fazy 1 na raz, żeby
agent zawsze miał jeden jasny, ograniczony krok.
