# OxideOS

Autorski, 64-bitowy system operacyjny pisany od zera w C++, z GUI w stylu Windows 95/98/2000. Architektura x86_64, bootowany przez [Limine](https://github.com/limine-bootloader/limine), uruchamiany w `qemu-system-x86_64`.

## Co już działa

- Własne jądro x86_64 (GDT/IDT/PIC/PIT, planista zadań, pamięć fizyczna/wirtualna)
- Sterownik **FAT32** z obsługą długich nazw plików (LFN)
- Aplikacje userspace jako osobne pliki `.ELF` w `/usr/bin`, uruchamiane w Ring 3 (Kalkulator, Notatnik, Panel Sterowania, Kalendarz, Paint, Menedżer Zadań, Zegar, WinVer)
- GUI: menedżer okien (Z-order), pasek zadań z systemowym trayem, **Menu Start odzwierciedlające na żywo zawartość `/usr/bin`**
- Kompozytor ekranu z alpha blendingiem, renderowanie BMP 24/32-bit
- Dźwięk przez AC97 (WAV)

## Budowanie i uruchamianie

Wymagania: `cmake`, `gcc`, `mtools`, `xorriso`, `qemu-system-x86_64`, `ImageMagick`.

```bash
./scripts/run.sh
```

Skrypt buduje jądro i aplikacje, generuje obraz dysku FAT32 (`disk.img`), konwertuje assety z `assets/` (PNG → BMP), tworzy `oxideos.iso` i odpala go w QEMU.

## Cel projektu

System ma wyglądać jak Windows 95/98/2000, ale być realnie używalny na co dzień. Miarą sukcesu jest 7-dniowy challenge korzystania wyłącznie z OxideOS (po sportowaniu przeglądarki, np. Ladybird z SerenityOS).

## Współpraca

Ten projekt jest rozwijany w duecie z Claude (Anthropic):

- **Kod** (jądro, sterowniki, GUI, syscalle, aplikacje) — Claude
- **Assety** (tapety, ikony, dźwięki, grafiki) i testowanie w QEMU — [Kocurowy96](https://github.com/Kocurowy96)
