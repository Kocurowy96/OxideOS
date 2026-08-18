OxideOS - Master Development Plan & AI Prompt

1. Założenia Projektowe (High-Level Overview)

Nazwa systemu: OxideOS

Główne języki: C++ (rdzeń systemu i natywne aplikacje), Python (dla aplikacji użytkownika).

Środowisko docelowe / testowe: QEMU (z obsługą wirtualizacji KVM, domyślnie architektura PC q35).

Architektura: x86_64 (64-bit).

Główne cele wizualne: Graficzny Interfejs Użytkownika (GUI) mocno inspirowany estetyką Windows 95/98 (szare okienka, ostre krawędzie, klasyczny pasek zadań z zasobnikiem i zegarem, czcionki bitmapowe).

2. Architektura Systemu i Jądro

Typ Jądra (Kernel): Hybrydowe / Mikrojądro.

Podstawowe funkcje (pamięć fizyczna/wirtualna, planista procesów, zarządzanie przerwaniami) działają w Ring 0 (tryb uprzywilejowany).

Większość sterowników (sieć, dyski) to odizolowane procesy (Ring 3).

Wyjątek wydajnościowy: Podsystem graficzny (Window Manager / Compositor) oraz obsługa urządzeń wejścia (mysz, klawiatura) są zintegrowane w jądrze (Ring 0) dla natychmiastowej responsywności UI, inspirując się ewolucją Windows NT 4.0.

Bootloader: Limine (nowoczesny protokół bootowania, tryb 64-bit od startu, Framebuffer sprzętowy, wsparcie dla UEFI i BIOS).

3. Pamięć Masowa, VFS i System Aktualizacji

Wirtualny System Plików (VFS): Warstwa abstrakcji unifikująca dostęp do różnych urządzeń i systemów plików (w tym integracja urządzeń jako plików).

System plików: ext4 jako docelowy system dla partycji dyskowych.

Układ Partycji:

boot - pliki bootloadera (Limine), plik jądra.

system - główny obraz systemu (nieużywany w trakcie normalnej pracy).

systemnow - partycja robocza, z której fizycznie operuje OS.

user - dane użytkownika (katalog domowy).

tmp - pliki tymczasowe w RAM (ramfs).

bin - zainstalowane pakiety i pliki konfiguracyjne (bin/config/<nazwapakietu>/).

recovery - narzędzia naprawcze.

Mechanizm Niezmiennych Aktualizacji (Immutable Updates):

Boot: OxideOS synchronizuje/kopiuje niezbędne dane z system do systemnow (jeśli wykryto nową wersję), po czym uruchamia OS z systemnow.

Praca: Partycja system jest odmontowana lub zablokowana na zapis dla standardowych procesów.

Aktualizacja: Menedżer Pakietów pobiera aktualizację i nadpisuje pliki bezpośrednio na "uśpionej" partycji system.

Restart: Przy kolejnym rozruchu system startuje z nowej, zaktualizowanej bazy z partycji system, powtarzając cykl.

4. Zarządzanie Procesami, IPC i Bezpieczeństwo

Użytkownicy i Uprawnienia: Architektura Multi-user. Konta z ograniczonymi prawami dla standardowych aplikacji (w tym sudo) oraz konto root.

Komunikacja Międzyprocesowa (IPC - Hybrydowa):

Message Passing: Do szybkich sygnałów (np. "otwórz plik", "kliknięto").

Shared Memory: Do transferu dużych bloków danych (np. dekodowanie grafiki, pakiety sieciowe).

Własny format plików wykonywalnych (.oxe):

Specjalny nagłówek rozpoznawany przez loader kernela (Magic Number, pozycje sekcji tekstowych/danych).

Zewnętrzne programy kompilowane są do tego formatu.

Ekosystem Pythona: Własny kompilator kompilujący czysty kod Pythona do natywnych binarek .oxe. Autorska biblioteka wewnątrz OxideOS do łatwego tworzenia okienek bez zewnętrznych zależności pip.

Oxidized Screen of Death (OSOD): Zintegrowany ekran błędu jądra (Kernel Panic) w klasycznym niebieskim kolorze (lub rdzawym!), zrzucający stan rejestrów i Call Stack na ekran do debugowania.

5. Graficzny Interfejs Użytkownika (GUI) i Multimedia

Grafika: LFB (Linear Framebuffer) z Limine. Natywne wsparcie dla dekodowania i renderowania plików graficznych w formacie `.bmp` (ikony, tła pulpitu).

Dźwięk: Sterownik SoundBlaster 16 lub Intel AC97 (pod QEMU) obsługujący autorskie dźwięki startowe ("startup chime" - surowe pliki PCM wave) i alerty systemowe. (Wskazówka: Pliki dźwiękowe zostaną dostarczone przez twórcę - nagrane ręcznie!).

Środowisko: Okna 3D, szare tła (#C0C0C0), systemowy Pasek Zadań (Taskbar) i Menu Start.

Czas i Zasobnik: Sterownik zegara czasu rzeczywistego (RTC/CMOS), zintegrowany z zasobnikiem systemowym (System Tray) w prawym dolnym rogu.

Wygaszacz Ekranu (Screensaver): Aktywujący się przy bezczynności (np. odbijające się rdzawoczerwone logo OxideOS na czarnym tle).

6. Sieć (Networking)

Sterownik: RTL8139 / E1000 (standard QEMU).

Stos sieciowy: Autorski implementowany od zera w izolowanym procesie: Ethernet -> IPv4 -> ARP / ICMP -> UDP / TCP -> DHCP.

7. Menedżer Pakietów (Packet Manager)

Przepływ instalacji:

Pobieranie struktury folderów/archiwum paczki ze zdalnego serwera do /tmp.

Weryfikacja.

Przeniesienie plików binarnych (.oxe) do /bin.

Ekstrakcja configów/assetów do /bin/config/<nazwapakietu>/.

8. Wbudowane Aplikacje i Gry

Kluczowe Systemowe:

Menedżer Zadań (Task Manager): Reagujący na Ctrl+Alt+Del, pokazujący użycie RAM i listę procesów z możliwością ich ubijania.

Panel Sterowania (Control Panel): Konfiguracja kolorów systemu, ustawienia sieci IP, czułość myszy, tło pulpitu.

Oxide Explorer (Przeglądarka internetowa): Podstawowy parser HTML/CSS napisany w C++, klient HTTP, renderowanie stron natywnymi elementami GUI OxideOS.

Programy Użytkowe: Eksplorator plików, Notatnik, Kalkulator, Terminal, OxidePaint (prosty klon MS Paint).

Narzędzia Deweloperskie: Oxide Studio (początkowo prosty edytor kodu Python z podświetlaniem składni, zintegrowany z kompilatorem .oxe).

Gry (Klasyki): Saper (Minesweeper), Pasjans (Solitaire) oraz Pinball.

9. Środowisko Budowania (Build System) i Warstwa Użytkownika

Build System: CMake + Make / Ninja. Umożliwia łatwą modularyzację projektu (kernel, moduły, aplikacje).

Kompilator: Cross-Compiler oparty na GCC/Clang (target x86_64-elf).

Biblioteka Standardowa (liboxide): Niezbędna warstwa (wrapper) pomiędzy C++ (np. printf, malloc) a autorskimi wywołaniami systemowymi (Syscalls) OxideOS.

10. Harmonogram i Etapy Rozwoju (Milestones)

WAŻNE: Ten harmonogram to długoterminowy projekt inżynieryjny (Continuous Development). Development ma przebiegać warstwa po warstwie, bez przeskakiwania do kolejnych faz przed pełną stabilizacją obecnej.

Faza 1: Bare Metal, Boot & Build System (FUNDAMENT)

Konfiguracja CMakeLists.txt, środowiska cross-kompilatora, skryptów uruchomieniowych QEMU.

Uruchomienie Limine, Framebuffer, i pierwsze pomyślne logi do portu szeregowego (COM1).

Faza 2: Kernel Core & Memory (SERCE)

GDT, IDT, obsługa przerwań, odczyt czasu (RTC).

Paging (pamięć wirtualna) i fizyczny alokator pamięci. Stabilność na poziomie 100%.

Faza 3: Multitasking, IPC & OSOD (MÓZG)

Planista (Scheduler), w pełni działająca izolacja procesów (Ring 3), obsługa Kernel Panic (OSOD).

Działający system wiadomości (Message Passing) i Shared Memory.

Faza 4: VFS, Storage & liboxide (PAMIĘĆ)

Wirtualny system plików, sterownik dysku IDE/AHCI, podstawowy odczyt ext4.

Zbudowanie i zlinkowanie liboxide.

Faza 5: Input & GUI Engine (CIAŁO - Ring 0)

Sterownik myszy i klawiatury (PS/2).

Natywny silnik rysujący (okna, przyciski, czcionki bitmapowe, Pasek Zadań).

Faza 6: Format .oxe & Ecosystem (EKOSYSTEM)

Rozpoznawanie nagłówka i ładowanie naszego formatu.

Integracja stosu i własnego kompilatora Pythona.

Faza 7: Network & Userland (KOMUNIKACJA)

Sterownik karty sieciowej (RTL8139/E1000), stos TCP/IP.

Implementacja menedżera pakietów (/tmp -> /bin).

Faza 8: Multimedia & Apps (DOŚWIADCZENIE)

Sterownik audio i obsługa autorskich dźwięków PCM.

Programy: OxidePaint, Pinball, Task Manager, Oxide Explorer.

Faza 9: Narzędzia Deweloperskie (Oxide Studio - v1)

Stworzenie środowiska IDE wewnątrz OxideOS (początkowo edytor tekstowy dla Pythona z integracją kompilacji do .oxe).

11. INSTRUKCJE KRYTYCZNE DLA AGENTA AI (Strict Development Rules)

Ten projekt ma symulować Prawdziwy, Inżynieryjny Development (Real-world Engineering). Zabraniam pośpiechu i generowania "zaślepek". Masz przestrzegać poniższych zasad:

Zasada "No Placeholders": Nigdy nie generuj kodu z komentarzami typu // TODO: Implement later lub // Add logic here po to, żeby szybciej przejść do kolejnej fazy. Jeśli piszesz funkcję alokatora pamięci, napisz ją od A do Z, dbając o edge-cases.

Krok po Kroku (Step-by-Step Execution): Pracuj w małych iteracjach (np. najpierw sam konfigurujesz CMake, testujemy, potem robisz pliki Limine, testujemy). Zatrzymuj się po zaimplementowaniu danej funkcjonalności i proś o przetestowanie kompilacji przez użytkownika. NIE WOLNO Ci wykonać całej Fazy w jednej odpowiedzi.

Pacing & Code Review: Traktuj użytkownika jako Senior Developera/Testera. Po napisaniu sterownika (np. myszy), wstrzymaj dalsze prace, wygeneruj skrypt testowy i poczekaj, aż użytkownik odpali go u siebie i potwierdzi, że działa.

Jakość i Bezpieczeństwo Jądra: W Ring 0 nie ma miejsca na błędy. Wszelkie wskaźniki (pointers) muszą być sprawdzane. W przypadku błędu kernela wywołuj zaprojektowany OSOD, zrzucając maksimum danych z rejestrów przez Serial Port (COM1), aby użytkownik mógł przesłać Ci logi do debugowania.

Modułowość: Utrzymuj rygorystyczną architekturę plików. Mieszanie kodu GUI z zarządzaniem pamięcią w jednym pliku kernel.cpp jest absolutnie zabronione.

12. Przyszłość i Roadmap (Długoterminowa Wizja)

OxideOS to projekt rozwojowy. Po osiągnięciu Fazy 9 planowane są:

Oxide Studio (v2 - Visual Builder): Wprowadzenie interfejsu Drag-and-Drop do Oxide Studio, umożliwiającego wizualne budowanie okien (w stylu Visual Basic/Delphi) i generowanie kodu Pythona pod spodem. Eliminuje to konieczność testowania w zewnętrznych maszynach wirtualnych (VMware/QEMU) podczas tworzenia apek.

Self-Hosting: Zdolność kompilowania kodu OxideOS (przez własny kompilator) wewnątrz uruchomionego systemu OxideOS.

Rozwój Sterowników: Dodawanie obsługi nowszego sprzętu (USB 2.0/3.0 Controller, nowoczesne karty graficzne).

Wsparcie Społeczności: Otwarcie API biblioteki liboxide dla zewnętrznych programistów i rozbudowa repozytoriów Menedżera Pakietów.

---
## Osiągnięto Milestone: Rozbudowa GUI (Window Manager, Ikony i Zamykanie) - ZAKOŃCZONA
Wprowadziliśmy Menedżera Okien do warstwy Compositora.
- OxideOS potrafi instancjonować obiekty klasy `Window`.
- Rysowanie respektuje Z-Index (tło -> okna -> pasek zadań -> mysz).
- Włączono przechwytywanie i przesuwanie okienek po ekranie (tzw. drag and drop) z wykorzystaniem trybu absolutnego dla kursora myszy.
- Okna potrafią się zamykać (znikać z tablicy menedżera) po kliknięciu wyrenderowanego przycisku **[X]**.
- OxideOS przy starcie ładuje z systemu plików maleńkie ikonki `.bmp` i dynamicznie rysuje je w Pasku Tytułowym!

## Co dalej?
Wszystkie najważniejsze podstawy systemu operacyjnego (GUI, Z-Index, Obsługa zdarzeń Myszki, Pamięć Masowa i VFS) zostały zbudowane na niesamowicie wysokim poziomie! 

Kolejnym potężnym krokiem w naturalnym cyklu rozwoju jest:
* **Przestrzeń Użytkownika (User Space - Ring 3)**: Oddzielenie pamięci jądra od pamięci użytkownika i pierwsze Syscalle. Prawdziwe załadowanie pierwszego, zewnętrznego programu wykonywalnego (np. w formacie `.elf` lub autorskim `.oxe`) z nowo stworzonego systemu FAT32 do pamięci wirtualnej i wykonanie go!
