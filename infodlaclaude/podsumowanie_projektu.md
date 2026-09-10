# Podsumowanie Projektu OxideOS

## Czym jest OxideOS?
OxideOS to autorski, 64-bitowy system operacyjny napisany od zera w C++. Posiada m.in.:
- System plików FAT32 (wraz z obsługą długich nazw plików - LFN)
- Interfejs graficzny (GUI) ze wsparciem dla wielozadaniowości, menedżera okien (Z-order) oraz paska zadań.
- Usprawniony i przyspieszony odczyt I/O dysku twardego ATA (czytanie wielosektorowe bezpośrednio do zmapowanej pamięci na strony HHDM).
- Kompozytor ekranu (Compositor) obsługujący przeźroczystość (Alpha Blending) dla kursora i grafik 32-bitowych ładowanych z dysku (np. cienie, wygładzanie).
- Zintegrowane renderowanie plików `.bmp` o formacie 24-bit i 32-bit.
- Stylizację klasyczną przypominającą Windows 95 (ciemnoniebieskie paski tytułowe, rzeźbione krawędzie 3D, klasyczne przyciski "Start").

## Co udało się dotychczas zbudować?
1. **FAT32 (LFN)** – System potrafi wczytywać pliki z partycji FAT32 z poprawnym odczytywaniem długich nazw plików (np. `cursor_normal.bmp`), zamiast trzymać się sztywnego formatu 8.3 (`CURSOR~1.BMP`). Używa wielosektorowego odczytywania klastrów ATA.
2. **Skrypt ułatwiający pracę (`scripts/run.sh`)** – Skrypt, który przy każdym odpaleniu tworzy kompilację środowiska (CMake), skanuje podłączony folder `assets` (Samba), a następnie w locie przetwarza wszystkie nowe ikony PNG użytkownika na natywny dla nas format 32-bitowy BMP poprzez ImageMagick.
3. **Menu Start** – Klasyczne menu, wystylizowane na Windows 95, zawierające ciemnoniebieski boczny baner "OxideOS". Dostosowane pod ładowanie natywnych ikon 32x32px prosto z dysku.
4. **Pasek Zadań (Taskbar) i System Tray** – Pasek śledzący wszystkie otwarte procesy/okna z prawidłowym przycinaniem nazw. Po prawej stronie wprowadzony zaszyty "Tray" wyświetlający zegar (czytający bezpośrednio z rejestru RTC procesora) i zarys ikon powiadomień (np. głośnika z pliku `icon_speaker.bmp`).
5. **Obsługa Okien i Z-Order** – Można otwierać, przeciągać myszką, minimalizować i maksymalizować okna (w tym proste wbudowane aplikacje jak Notatnik, Ustawienia, Kalkulator czy WinVer).

## Architektura i Kontekst Techniczny
- Budowa poprzez `CMake`. Główny obraz to `kernel.elf`, montowany przez skrypt jako CD-ROM/Disk.
- Uruchamiamy projekt przy użyciu `qemu-system-x86_64`.
- Zewnętrzne assetsy (np. tapety, kursory, ikony do menu) wrzucane są na współdzielony dysk sieciowy (Samba: `//192.168.0.133/KocurDrive/OxideOS/assets`), gdzie system automatycznie podbiera pliki PNG, konwertuje do 32-bitowego BMP i zaszywa na dysku `disk.img`.

## Plany na przyszłość
Szczegóły znajdują się w pliku `roadmap_i_plany.md` wyeksportowanym z naszych wcześniejszych sesji deweloperskich. Obejmują one m.in.:
- Rozbudowę Paska Powiadomień (dodawanie kolejnych dynamicznych ikon stanu z odpowiednim odstępem).
- Rozwój interfejsu (dodawanie widżetów/ikonek w oknach).
