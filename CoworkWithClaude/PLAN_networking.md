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
- **Pierwszy namacalny kamień milowy: odpowiedź na `ping`.** Analogicznie do "Ext2:
  Initialized successfully" — jasny, łatwo weryfikowalny moment "działa": `ping 10.0.2.15` z
  hosta podczas gdy OxideOS działa w QEMU dostaje prawdziwe odpowiedzi (nie "Destination
  unreachable"). Wymaga kompletnego, minimalnego stosu w obie strony: sterownik RTL8139 (TX+RX)
  + Ethernet + ARP + IP + ICMP.
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

**Faza 1 — sterownik RTL8139 (wykrycie + tożsamość karty):**
1a. `PCI::FindDevice(0x10EC, 0x8139, ...)`, odczyt BAR0 (I/O port base), power-on (rejestr
    Config1), soft reset (rejestr CR, bit RST), odczyt adresu MAC z rejestrów ID0-ID5. Log
    diagnostyczny z odczytanym MAC — pierwszy namacalny dowód że sterownik w ogóle "widzi"
    kartę. Weryfikacja: log porównany z MAC-iem który QEMU sam przydzielił karcie (widoczny w
    logu QEMU albo przez `-device rtl8139,netdev=net0,mac=52:54:00:12:34:56` jeśli wolimy
    ustalić go z góry zamiast losowego).
1b. Włączenie odbiornika/nadajnika (rejestr CR, bity RE/TE), konfiguracja bufora RX (rejestr
    RBSTART — jeden ciągły bufor ~8KB+16, tryb "accept all"/broadcast na start), IRQ (maska
    w IMR, rejestr ISR do potwierdzania przerwań, wpis do PIC jak inne sterowniki).

**Faza 2 — surowe ramki Ethernet (TX/RX, bez interpretacji wyżej):**
2a. TX pojedynczej, ręcznie złożonej ramki testowej (np. broadcast, dowolna zawartość) przez
    jeden z 4 deskryptorów TX (TSAD/TSD). Weryfikacja: `tcpdump -r dump.pcap` na hoście
    pokazuje dokładnie tę ramkę.
2b. RX — obsługa przerwania odbioru, odczyt ramki z bufora cyklicznego, log surowych bajtów
    nagłówka Ethernet (adresy MAC + EtherType). Weryfikacja: wygenerować dowolny ruch do
    `10.0.2.15` z hosta (np. `ping`, zanim jeszcze mamy IP — dostaniemy tylko ARP request, ale
    to już realna ramka do zalogowania) i porównać z `tcpdump`.

**Faza 3 — ARP:** odbiór ARP request (kto ma `10.0.2.15`?) i wysłanie ARP reply z naszym MAC;
wysłanie własnego ARP request do bramy (`10.0.2.2`) i sparsowanie odpowiedzi — potrzebne żeby
w ogóle wysłać cokolwiek poza segment. Prosta tablica ARP (statyczna, mały rozmiar, bez
wygasania na start — wystarczy na czas testów).

**Faza 4 — IP + ICMP (KAMIEŃ MILOWY):** budowa/parsowanie nagłówka IPv4 (bez fragmentacji na
start, suma kontrolna), ICMP echo request/reply. Weryfikacja: `ping 10.0.2.15` z hosta podczas
działania OxideOS w QEMU dostaje prawdziwe odpowiedzi (czas round-trip, nie timeout/unreachable).

**Faza 5 — UDP:** nagłówek UDP + prosty test (np. echo na stałym porcie, weryfikowany przez
`nc -u 10.0.2.15 <port>` albo mały skrypt Python po stronie hosta).

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
