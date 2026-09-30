# Raport: Sieć Faza 6a (DHCP)

**Data:** 26-09-2026 22:45 (czasu polskiego)
**Gałąź:** main-0k7z9q
**Commit:** (patrz `git log` — commit tworzony razem z tym raportem)

## Zadanie
Początek dłuższej, samodzielnej nocnej sesji (Kocurowy96 idzie spać) po zamknięciu Fazy 5
(UDP). Cel ogólny: dociągnąć sieć możliwie najdalej, każdy krok jako osobna, w pełni
zweryfikowana faza z osobnym commitem — zatrzymać się na ostatniej czysto zamkniętej fazie
jeśli coś się nie uda lub zabraknie czasu, nie zostawiać niedokończonego kodu.

Faza 6a konkretnie: zastąpić statyczny `NET_OUR_IP` prawdziwym DHCP
(DISCOVER→OFFER→REQUEST→ACK) na UDP portach 67/68, ten sam wzorzec co ARP/IP/ICMP/UDP
(nowy `kernel/net/dhcp.h/cpp`), weryfikacja przez `tcpdump` (SLIRP ma wbudowany serwer
DHCP, ma zadziałać bez dodatkowych flag QEMU).

## Synchronizacja z main
`git fetch origin && git merge origin/main` na starcie — jeden nowy commit
(`a8db92c`, przepisanie README na angielski + nowy `docs/` z dokumentacją architektury,
niezwiązane z siecią). Fast-forward, zero konfliktów.

## Co zrobiono
Nowy `kernel/net/dhcp.h/cpp`: `DHCP::Run()` (blokujący, wywoływany raz z
`kernel/main.cpp` zaraz po `RTL8139::Init()`, PRZED prewarmem ARP który zależy od
prawdziwej bramy) realizuje pełny cykl DISCOVER→OFFER→REQUEST→ACK.

**`kernel/net/config.h`'s `NET_OUR_IP`/`NET_GATEWAY_IP` przestały być zaszytymi na
sztywno `static const`** — teraz mutowalne `extern uint8_t[4]` (definicje w nowym
`config.cpp`, bo mutowalne globalne nie mogą już być kopiowane do każdej jednostki
kompilacji jak przy starym `static const`). Zaczynają jako `{0,0,0,0}`;
`DHCP::Run()` wypełnia obie prawdziwymi wartościami z ACK (adres z pola `yiaddr`, brama
z opcji 3 "Router" w odpowiedzi serwera — z fallbackiem na Server-ID gdyby serwer jej nie
podał). Świadome uproszczenie: klient działa **raz przy starcie kernela**, bez
odnawiania dzierżawy (brak śledzenia czasu jej trwania) — jak `critical.h`, zaakceptowany
dług techniczny, nie przeoczenie; dopisane wprost w komentarzu nagłówkowym `dhcp.h`.

**Dwie realne, konieczne zmiany w już istniejącym, zweryfikowanym kodzie Faz 3-4** —
nie kosmetyczne, tylko blokery bez których DHCP fizycznie nie mogło zadziałać:
1. `IP::Send` zawsze wymagało rozwiązania MAC celu przez ARP przed wysłaniem czegokolwiek.
   DHCP musi wysyłać na `255.255.255.255` (rozgłoszeniowo) — klient nie ma jeszcze
   żadnego adresu ani bramy, więc pytanie ARP-em "kto ma adres rozgłoszeniowy" nie ma
   sensu. Dodana wczesna gałąź w `IP::Send`: jeśli `dst_ip` to `255.255.255.255`, użyj
   wprost rozgłoszeniowego adresu Ethernet (`FF:FF:FF:FF:FF:FF`), pomijając ARP
   całkowicie. **Zero zmiany zachowania dla wszystkich dotychczasowych wywołań** — ICMP
   i UDP zawsze przekazywały (i nadal przekazują) realne adresy jednostkowe, ta gałąź
   nigdy ich nie dotyczy.
2. `UDP::HandleFrame` dostało trzecią gałąź w dispatchu (obok już istniejącego portu 7
   echo): port 68 (port klienta DHCP) kieruje do `DHCP::HandleFrame`.

**Nieoczywisty błąd stanu, zdiagnozowany na etapie projektowania, nie znaleziony po
fakcie**: pierwsza wersja automatu stanu DHCP (jedna zmienna `g_state` ustawiana przez
`HandleFrame`, odpytywana przez `Run()`) miała realny, choć rzadki bug — spóźniony albo
powtórzony `OFFER` odebrany PODCZAS czekania na `ACK` ustawiłby stan tak, że pętla
czekająca na `ACK` błędnie uznałaby to za nadejście odpowiedzi (obie fazy dzieliły ten
sam stan bez rozróżnienia "na co właśnie czekamy"). Naprawione przez jawną bramkę
`g_waiting` (enum `NONE`/`OFFER`/`ACK`) — `HandleFrame` aktualizuje stan tylko gdy typ
odebranego komunikatu pasuje do tego na co `Run()` faktycznie w danym momencie czeka, w
przeciwnym razie po cichu ignoruje pakiet. Stan oznaczony `volatile` (ten sam powód co
`g_critical_depth` w `critical.h`) — zapis z przerwania RX, odczyt w pętli spinowej,
kompilator nie może założyć że się nie zmieni między iteracjami.

`CMakeLists.txt`: `kernel/net/config.cpp` i `kernel/net/dhcp.cpp` dopisane do
`KERNEL_SOURCES`.

## Weryfikacja
- Pełny czysty rebuild: **PASS, 0 błędów, 0 nowych ostrzeżeń.**
- `scripts/test_headless.sh`: **PASS** — log pokazuje pełny, poprawny cykl:
  `DHCP: sent DISCOVER.` → `DHCP: received OFFER for 10.0.2.15` → `DHCP: sent REQUEST.`
  → `DHCP: bound, IP=10.0.2.15 gateway=10.0.2.2` → prewarm ARP dla bramy dalej działa
  poprawnie zaraz po tym (potwierdza że kolejność inicjalizacji — DHCP przed ARP prewarm
  — jest bezpieczna).
- **Niezależnie potwierdzone przez `tcpdump -r net_dump.pcap -v`** (dissector `tcpdump`
  sam rozpoznaje protokół BOOTP/DHCP, nie trzeba było ręcznie interpretować surowych
  bajtów) — cztery pakiety dokładnie odpowiadające logowi kernela:
  - `Discover` (xid `0x39a17b2c`, zgodny z logiem; `Parameter-Request: Default-Gateway`
    — potwierdza że moja opcja 55 faktycznie dotarła w takiej postaci jak zbudowana)
  - `Offer` (`Your-IP 10.0.2.15`, `Server-ID 10.0.2.2`, `Default-Gateway 10.0.2.2`)
  - `Request` (`Requested-IP 10.0.2.15`, `Server-ID 10.0.2.2` — poprawnie odesłane
    dokładnie to co przyszło w Offer, zgodnie z RFC 2131)
  - `ACK` (te same wartości potwierdzone przez serwer)

  Zero rozbieżności między logiem kernela a ruchem widocznym na drucie.
- `tcpdump` (zainstalowane w poprzedniej sesji, wciąż dostępne w tym kontenerze) użyte
  bez dodatkowych flag QEMU — SLIRP-owy serwer DHCP zadziałał "za darmo", zgodnie z
  przewidywaniem w treści zadania.

## Napotkane problemy / obserwacje
Zero problemów przy samej implementacji — jedyna rzecz wymagająca realnego namysłu (nie
zgadywania) to błąd stanu opisany wyżej, złapany PRZED napisaniem finalnego kodu (podczas
projektowania automatu stanu), nie odkryty przez debugowanie później.

## Co zostało / kolejne kroki
- Zgodnie z instrukcją: Faza 6a to osobny, zamknięty commit. Kontynuacja od razu do Fazy
  6b (TCP) w tej samej nocnej sesji, na wyraźny "start" już dany.
- DHCP nie odnawia dzierżawy (świadome uproszczenie, opisane wyżej) — jeśli kiedyś okaże
  się to potrzebne (długo działający system, prawdziwy sprzęt z krótkim leasem), do
  rozszerzenia osobno.
- `TASKS.md` zaktualizowane: Faza 6a przeniesiona do "Zrobione" z pełnym opisem, "Do
  zrobienia teraz"/"Do przegadania" zaktualizowane pod kątem trwającej sesji TCP.

## Push
Zmiany (`kernel/net/dhcp.h`, `kernel/net/dhcp.cpp`, `kernel/net/config.h`,
`kernel/net/config.cpp`, `kernel/net/ip.cpp`, `kernel/net/udp.cpp`, `kernel/main.cpp`,
`CMakeLists.txt`, `CoworkWithClaude/TASKS.md`, ten raport) do wypchnięcia na
`origin/main-0k7z9q` razem z tym commitem (nie na `main`).
