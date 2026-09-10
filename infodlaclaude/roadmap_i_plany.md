# FAT32 Long File Name (LFN) Support

Dodanie obsługi długich nazw plików (do 255 znaków) w sterowniku FAT32 OxideOS.

## Kontekst problemu

Aktualny driver obsługuje tylko nazwy w formacie **8.3** (max 8 znaków + 3 rozszerzenie, wielkie litery).  
Przez to pliki z długimi nazwami (np. `cursor_normal.bmp`, `wp1_thumb.bmp`) są widoczne na dysku jako `CURSOR~1.BMP`, `WP1_TH~1.BMP` i VFS nie może ich znaleźć po prawdziwej nazwie.

## Jak działa FAT32 LFN

Standard FAT32 LFN (Long File Name) przechowuje długie nazwy w specjalnych wpisach katalogu tuż **przed** zwykłym wpisem 8.3:

```
[LFN entry #2] → "mal.bmp\0\0\0\0\0"  (ostatnie znaki)
[LFN entry #1] → "cursor_nor"          (pierwsze znaki)  
[SFN entry]    → "CURSOR~1BMP"         (nazwa 8.3, startowy klaster, rozmiar)
```

Każdy wpis LFN (32 bajty, atrybut `0x0F`):
| Offset | Rozmiar | Opis |
|--------|---------|------|
| 0      | 1       | Numer porządkowy (bit 6 = ostatni wpis) |
| 1–10   | 10      | Znaki 1–5 (UTF-16LE) |
| 11     | 1       | Atrybut = 0x0F |
| 13     | 1       | Suma kontrolna (checksum) SFN |
| 14–25  | 12      | Znaki 6–11 (UTF-16LE) |
| 28–31  | 4       | Znaki 12–13 (UTF-16LE) |

## Proponowane zmiany

---

### FAT32 Driver

#### [MODIFY] [fat32.cpp](file:///home/kocurowy96/Obrazy/OxideOS/kernel/fs/fat32.cpp)

Główne zmiany:

1. **Nowa struktura `LFNEntry`** — 32-bajtowy wpis LFN
2. **Funkcja `extract_lfn_chars()`** — wyciąga 13 znaków UTF-16LE z wpisu LFN i konwertuje do ASCII (litery spoza ASCII → `?`)
3. **Funkcja `assemble_lfn()`** — składa pełną nazwę z kolejności wpisów LFN
4. **Funkcja `str_eq_lfn()`** — porównuje zebraną nazwę LFN z szukaną (case-insensitive)
5. **Modyfikacja pętli skanowania katalogów** — zamiast `continue` przy `0x0F`, zbiera wpisy LFN w buforze, a przy następnym SFN sprawdza zarówno nazwę 8.3 jak i LFN

Pętle skanujące są w 3 miejscach:
- `FindDirectoryCluster()` — szukanie podkatalogu
- `FAT32::ReadFile()` — szukanie pliku do odczytu  
- `FAT32::WriteFile()` / `FAT32::CreateFileInDirectory()` — szukanie/tworzenie pliku

---

## Plan implementacji (kolejność)

1. Dodaj helper `extract_lfn_chars()` — konwersja UTF-16LE → ASCII
2. Dodaj `assemble_lfn()` + `str_eq_lfn()`  
3. Zmodyfikuj pętlę w `ReadFile()` — zbieranie LFN i porównanie
4. Zmodyfikuj pętlę w `FindDirectoryCluster()` — to samo dla podkatalogów
5. (Opcjonalnie) `WriteFile()` — dla zapisu wystarczy 8.3 na razie

## Co NIE wchodzi w zakres

- **Zapis z LFN** — do tworzenia nowych plików nadal używamy 8.3 (wystarczy na teraz)
- **Unicode >127** — konwertujemy do ASCII, polskie znaki → `?` (można rozszerzyć później)
- **Wielki refactor** — minimalne zmiany, tylko to co potrzebne

## Weryfikacja

Po wdrożeniu:
- `cursor_normal.bmp` → powinno załadować się poprawnie
- `wp1_thumb.bmp` → miniatura tapetki widoczna w ustawieniach
- Stare krótkie nazwy (np. `bg.bmp`, `icon.bmp`) nadal działają (8.3 fallback)

---

## Kolejne Kroki / Zaległe Zadania
- [x] **FAT32 Long File Name (LFN)**
- [x] **Menu Start**: 
  - [x] Ostylowanie (pasek boczny, kolory)
  - [x] Ikony 32x32
- [x] **Optymalizacja tapet (I/O dysku)**: Zrobione (DMA/Multi-sector).
- [ ] **Implementacja paska powiadomień/tray'a**: (Aktualne zadanie)
