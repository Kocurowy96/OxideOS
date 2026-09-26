# Plan: stos sieciowy (Ethernet → ARP → IP → ICMP → UDP → TCP)

Ustalone 2026-09-26/27 w nocy. Zero sieci w OxideOS dziś — to największy pojedynczy brakujący
podsystem (większy zakresem niż cała migracja ext2). Rozbijamy fazowo z tego samego powodu co
ext2: żeby codzienny agent w chmurze mógł to ciągnąć krok po kroku, a nie próbować zrobić
wszystko naraz.

## Kluczowe decyzje

- **Karta sieciowa: RTL8139.** Klasyczny wybór w hobby-OS — PIO + jeden duży bufor RX (nie
  nowoczesny pierścień deskryptorów jak e1000/virtio), mnóstwo dokumentacji (OSDev Wiki ma
  osobną, szczegółową stronę). Podobny poziom trudności co już mamy z AC97 (PCI + BAR + IRQ).
  Odrzucone: e1000 (bardziej złożona inicjalizacja bez wyraźnej korzyści), virtio-net
  (parawirtualizacja to inna warstwa abstrakcji, mniej pasuje do stylu projektu — prawdziwy
  ATA PIO i prawdziwy AC97, nie ich virtio-odpowiedniki).
- **QEMU: tryb `user` (SLIRP), nie `tap`.** Nie wymaga roota/konfiguracji mostka sieciowego na
  hoście, daje gotowe NAT + serwer DHCP-podobny (gość dostaje `10.0.2.15`, brama `10.0.2.2`,
  DNS `10.0.2.3` — domyślna podsieć QEMU user-mode). Do weryfikacji: `-object
  filter-dump,id=f1,netdev=net0,file=dump.pcap` przechwytuje cały ruch do pliku, czytelny
  potem przez `tcpdump -r dump.pcap` na hoście — **ten sam wzorzec "porównaj z zaufanym
  narzędziem hosta"** co `debugfs`/`e2fsck` przy ext2. `tcpdump` potwierdzone dostępne
  lokalnie.
- **Statyczne IP na start, nie DHCP.** `10.0.2.15/24`, brama `10.0.2.2` — zaszyte w kodzie.
  DHCP to osobny protokół (UDP + specyficzny format pakietów) który sam w sobie odwlekałby
  pierwszy namacalny efekt — dorzucić później, jeśli kiedyś faktycznie potrzebne (np. do
  prawdziwego sprzętu, gdzie statyczne IP nie ma sensu).
- **Pierwszy namacalny kamień milowy: OxideOS samo wysyła `ping` i dostaje prawdziwą
  odpowiedź.** **KOREKTA z 2026-09-26/27 (zweryfikowane empirycznie przed Fazą 2b, nie
  zgadywane):** pierwotne założenie "`ping 10.0.2.15` z hosta" jest fałszywe przy QEMU
  `-netdev user` — podsieć `10.0.2.0/24` jest wewnętrzna wyłącznie dla procesu QEMU (libslirp),
  host nie ma do niej trasy routingu. Sprawdzone bezpośrednio: `ping 10.0.2.15` z prawdziwego
  hosta (Kocurowy96'a maszyna) podczas działania OxideOS w QEMU z `-netdev user` zwraca
  **"Destination Net Unreachable"** natychmiast (odrzucone przez stos sieciowy HOSTA, zanim
  cokolwiek dotarlo do QEMU — `net_dump.pcap` w tej samej próbie pozostał pusty, 0 pakietów,
  potwierdzając że nic nie opuściło hosta). **Kierunek kamienia milowego odwrócony:** to
  OxideOS ma wysłać ICMP echo request do bramy (`10.0.2.2` — libslirp odpowiada na ping do
  własnego adresu bramy z założenia, do celów diagnostycznych) albo realnego hosta w
  internecie (np. `8.8.8.8`, NAT-owane przez libslirp) i dostać prawdziwą odpowiedź z powrotem
  — to jest właściwy, standardowy sposób testowania łączności w trybie `user`. Weryfikacja:
  log w kernelu pokazujący odebraną odpowiedź (adres, TTL, czas) + `tcpdump -r net_dump.pcap`
  pokazujący realną wymianę request/reply na drucie. Wymaga kompletnego, minimalnego stosu w
  obie strony: sterownik RTL8139 (TX+RX) + Ethernet + ARP + IP + ICMP.
- **Do przyszłych faz (UDP/TCP, gdy testy mają iść w drugą stronę — host łączy się DO
  OxideOS):** `-netdev user` **wspiera** przekierowanie portów hosta do gościa
  (`hostfwd=tcp::PORT-:GUESTPORT` / `udp::PORT-:GUESTPORT`, dopisywane do `-netdev user,...`),
  w odróżnieniu od gołego ICMP. To będzie właściwy mechanizm do testowania np. serwera UDP/TCP
  w OxideOS przez `nc`/skrypt hosta w Fazach 5-6 — nie trzeba do tego trybu `tap`.
- **Kolejność UDP przed TCP.** UDP jest drastycznie prostszy (brak automatu stanów połączenia,
  brak retransmisji/okna) — dobry pośredni krok potwierdzający warstwę IP zanim ruszy dużo
  większa złożoność TCP.
- **Zakres TCP: świadomie ograniczony.** Pełny TCP (retransmisje z adaptacyjnym RTO, kontrola
  przeciążenia, duże okna, SACK) to osobny, wielotygodniowy projekt sam w sobie. Na start:
  pojedyncze połączenie na "czystym" wirtualnym łączu (QEMU user-mode, brak realnej utraty
  pakietów) — prosty handshake/dane/zamknięcie, podstawowa retransmisja przy timeout, bez
  wyrafinowanej kontroli przeciążenia. Świadome uproszczenie w stylu `critical.h` — nie
  "brakujące", tylko zaakceptowany dług techniczny, do rozszerzenia tylko jeśli kiedyś się
  okaże że faktycznie potrzebne.

## Struktura kodu

Nowy katalog `kernel/net/` (analogicznie do `kernel/fs/`): `ethernet.h/cpp`, `arp.h/cpp`,
`ip.h/cpp`, `icmp.h/cpp`, `udp.h/cpp`, `tcp.h/cpp`. Sterownik karty: `kernel/drivers/rtl8139.h/cpp`
(wzorem `kernel/drivers/ac97.h/cpp` — PCI detect przez już istniejące `PCI::FindDevice`, IRQ
przez PIC, ten sam styl).

## Zmiana w skryptach QEMU (Faza 0, przed jakimkolwiek kodem sterownika)

Każdy skrypt uruchamiający QEMU (`run.sh`, `test_headless.sh`, `screendump.sh`,
`headless_interact.sh`, `gdb_inspect.sh`) dziś osobno buduje własne flagi — trzeba dopisać do
każdego coś w rodzaju:
```
-netdev user,id=net0 -device rtl8139,netdev=net0 -object filter-dump,id=f1,netdev=net0,file=net_dump.pcap
```
*Do przegadania przy pierwszej fazie:* czy dopisywać to osobno w każdym skrypcie (zgodnie z
dzisiejszym stylem "każdy skrypt niezależny"), czy to dobry moment na wspólny plik z flagami
QEMU (`scripts/qemu_flags.sh`, source'owany przez wszystkie) — dotąd unikaliśmy takiej
abstrakcji, ale liczba miejsc do zsynchronizowania rośnie z każdym nowym urządzeniem.

## Fazy

**Faza 0 — ten dokument + decyzje.** Zrobione.

**Faza 1 — sterownik RTL8139 (wykrycie + tożsamość karty). ZROBIONE 2026-09-26/27.**
1a. Zrobione. `PCI::FindDevice(0x10EC, 0x8139, ...)`, BAR0, power-on, soft reset, odczyt MAC —
    log porównany z monitorem QEMU (`info pci`), zgodny.
1b. Zrobione. RX/TX enable, bufor RX (RBSTART), RCR (accept-all+WRAP), IMR (ROK+TOK), IRQ
    (linia odczytana z PCI config w runtime, nie stała) — zweryfikowane przez monitor QEMU
    (`info pci` + bezpośredni odczyt rejestru CR portem I/O).

**Faza 2 — surowe ramki Ethernet (TX/RX, bez interpretacji wyżej). ZROBIONE 2026-09-26/27.**
2a. Zrobione. TX pojedynczej, ręcznie złożonej ramki testowej (broadcast, EtherType 0x88B5)
    przez deskryptor TX0 (TSAD0/TSD0). Zweryfikowane: `tcpdump -r net_dump.pcap` pokazuje
    ramkę identyczną bajt-po-bajcie (adresy MAC, EtherType, payload, dopełnienie do 60B).
    **UWAGA znaleziona dopiero w Fazie 4:** zawsze-TX0 działa dla POJEDYNCZEJ transmisji, ale
    powtórne użycie TEGO SAMEGO deskryptora zaraz po jego własnym zakończeniu kończyło się
    TX timeout — poprawione rotacją między 4 deskryptorami/buforami, patrz Faza 4 niżej.
2b. Zrobione. RX — obsługa przerwania odbioru (`RTL_ISR_ROK`), odczyt nagłówka pakietu z
    bufora cyklicznego (2B status + 2B długość WŁĄCZNIE z 4B CRC, potem dane), aktualizacja
    CAPR (potwierdzony hardware quirk RTL8139: CAPR = pozycja odczytu MINUS 16, nie wprost).
    Zweryfikowane przez `hostfwd`+`curl` wymuszające realny ruch przychodzący (ARP request od
    SLIRP), porównane z `tcpdump` — identyczne.

**Faza 3 — ARP. ZROBIONE 2026-09-26/27.** Nowy katalog `kernel/net/` (ethernet.h/cpp,
arp.h/cpp). Wysłanie własnego ARP request do bramy + parsowanie odpowiedzi, prosta statyczna
tablica ARP (8 wpisów, "gratuitous learning"), odpowiedź na cudzy request do naszego adresu.
Zweryfikowane w pełni autonomicznie (bez `hostfwd`) — `tcpdump` potwierdza request/reply
identyczne z logiem kernela.

**Faza 4 — IP + ICMP (KAMIEŃ MILOWY, kierunek odwrócony — patrz korekta w "Kluczowe
decyzje"). OSIĄGNIĘTA 2026-09-26/27.** `kernel/net/ip.h/cpp` + `icmp.h/cpp` — budowa/
parsowanie nagłówka IPv4 (bez fragmentacji, suma kontrolna RFC 1071), ICMP echo request/
reply. **Po drodze znaleziony i naprawiony realny bug z Fazy 2a:** zawsze-TX0 nie nadawał
się do WIĘCEJ NIŻ jednej transmisji pod rząd (drugie użycie tego samego deskryptora →
TX timeout, odtworzone syntetycznie wysyłając ten sam ARP dwukrotnie) — naprawione rotacją
między 4 deskryptorami/buforami TX. Zweryfikowane: OxideOS wysyła ICMP echo request do
`10.0.2.2`, dostaje prawdziwy echo reply, w pełni autonomicznie (bez `hostfwd`) —
`tcpdump -r net_dump.pcap -v` potwierdza dokładnie zgodną wymianę request/reply
(id/seq/długość). Checkpoint przed Fazą 6 — patrz niżej.

**Faza 5 — UDP:** nagłówek UDP + prosty test. Ponieważ `ping`-podobny test "OxideOS łączy się
NA ZEWNĄTRZ" nie sprawdza się dobrze dla UDP (brak prostego, zawsze-dostępnego usługowego
"echo" jak ICMP), lepszy test w drugą stronę: prosty serwer UDP echo w OxideOS + `-netdev
user,...,hostfwd=udp::PORT-:GUESTPORT` + `nc -u localhost <PORT>` z hosta (patrz korekta w
"Kluczowe decyzje" — `hostfwd` działa dla UDP/TCP, w odróżnieniu od gołego ICMP).

**Faza 6 — TCP (rozbić dalej przy starcie tej fazy, nie z góry):** orientacyjnie handshake
(SYN/SYN-ACK/ACK) → przesył danych z poprawnymi seq/ack (bez retransmisji) → podstawowa
retransmisja przy timeout → zamknięcie (FIN/ACK). Świadomie płytszy zakres niż "prawdziwy"
TCP (patrz decyzje wyżej) — **checkpoint po Fazie 4/5:** zdecydować razem czy i jak głęboko
wchodzić w TCP, zamiast zakładać z góry pełny zakres.

**Faza 7 — API dla aplikacji:** syscalle w stylu gniazd (`sys_net_connect`/`send`/`recv`/
`close` czy podobne — szczegóły do ustalenia bliżej tej fazy, nie teraz) + jakaś prosta apka
demo (np. "Ping" albo pobranie małego zasobu z hosta). Zbyt odległe żeby projektować szczegóły
teraz.

## Jak to wejdzie do TASKS.md

Analogicznie do ext2: do "Do zrobienia teraz" trafia na start tylko **Faza 1a** (wykrycie
karty + odczyt MAC, zero jeszcze TX/RX) — wąska, samodzielnie weryfikowalna, zero integracji z
resztą systemu. Kolejne podpunkty dopisywane sukcesywnie po zamknięciu poprzedniego.
