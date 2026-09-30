# Raport: Sieć Faza 6c (klient HTTP/1.0) — zamknięcie nocnej sesji sieciowej

**Data:** 26-09-2026 23:50 (czasu polskiego)
**Gałąź:** main-0k7z9q
**Commit:** (patrz `git log` — commit tworzony razem z tym raportem)

## Zadanie
Ostatni krok tej samodzielnej nocnej sesji, TYLKO uruchomiony bo Faza 6b (TCP) w pełni
zadziałała i została zweryfikowana (warunek jawnie postawiony przez Kocurowy96). Zakres:
minimalny klient HTTP/1.0 GET na bazie TCP z Fazy 6b — jedno żądanie, bez chunked
encoding, bez TLS, bez utrzymywania połączenia — zweryfikowany realnym pobraniem czegoś
małego z lokalnego serwera HTTP przez odpowiedni mechanizm przekierowania QEMU.

## Co zrobiono
Nowy `kernel/net/http.h/cpp`: `HTTP::Get(dst_ip, dst_port, host, path, out_buf,
out_buf_len)`. Buduje ręcznie żądanie (`GET <path> HTTP/1.0\r\nHost: <host>\r\n
Connection: close\r\n\r\n`, z prostym, bezpiecznym `AppendStr` saturującym zamiast
przepełniać bufor przy zbyt długich parametrach), wysyła przez `TCP::SendData`, czyta
odpowiedź w pętli wołającej `TCP::Receive` aż zwróci 0 (EOF), zamyka połączenie przez
`TCP::Close`. Świadomie minimalny, dokładnie w zakresie z zadania: brak parsowania
`Content-Length` (po prostu czyta do EOF), brak obsługi chunked encoding, brak TLS, brak
utrzymywania połączenia (`Connection: close` w żądaniu). Zbudowany WPROST na już
istniejącym, blokującym `TCP::` z Fazy 6b — zero zmian w `tcp.h`/`tcp.cpp`.

## Weryfikacja
Realnym pobraniem z prawdziwego serwera HTTP na hoście — `python3 -m http.server`,
dokładnie jak sugerowało zadanie — serwującego mały plik tekstowy (`hello.txt`, 73
bajty treści). Ten sam mechanizm co przy weryfikacji Fazy 6b: `guestfwd` (nie
`hostfwd` — patrz korekta odnotowana w raporcie Fazy 6b) przez mały most (`nc
127.0.0.1 8000`) łączący połączenie inicjowane przez gościa z prawdziwym, niezależnie
uruchomionym serwerem Pythona.

**Potwierdzone z TRZECH niezależnych źródeł jednocześnie**:
1. **Log kernela**: `HTTP TEST: received 260 bytes` z pełną, poprawną treścią —
   nagłówki HTTP (`HTTP/1.0 200 OK`, `Content-Length: 73`, `Server: SimpleHTTP/0.6
   Python/3.11.15` itd.) i dokładna treść testowego pliku po pustej linii rozdzielającej.
2. **Log serwera Pythona** (niezależny proces na hoście): `"GET /hello.txt HTTP/1.0"
   200 -`, z tą samą sekundą (`21:56:22`) co nagłówek `Date` w odpowiedzi widocznej w
   logu kernela — potwierdza że to naprawdę ten sam request/response, nie zbieg
   okoliczności.
3. **`tcpdump -n -v`** na zrzucie ruchu — **każdy pojedynczy pakiet ma `cksum ...
   (correct)`**: 64-bajtowe żądanie GET, 260-bajtowa odpowiedź, kompletna poprawna
   sekwencja SYN/SYN-ACK/ACK → dane w obie strony (z ACK po każdym) → FIN-ACK od nas →
   ACK → FIN-ACK od serwera → ACK finalny.

**Uczciwie odnotowana obserwacja, nie ukryta pod dywan**: w logu kernela pojawiło się
jedno `TCP: receive timeout.` mimo że transfer się w pełni powiódł. Przyczyna
(potwierdzona przez znaczniki czasu w `tcpdump`): most `nc` między SLIRP a serwerem
Pythona nie propagował zamknięcia połączenia proaktywnie zaraz po wysłaniu odpowiedzi
(mimo `Connection: close` w nagłówkach) — dopiero gdy OxideOS samo wysłało swój FIN
(po ~80ms oczekiwania w `TCP::Receive`, dokładnie tyle ile wynosi wbudowany limit
czekania), serwer/most odpowiedział własnym FIN. To **funkcjonalnie bez znaczenia** —
`HTTP::Get` i tak wywołuje `TCP::Close()` niezależnie od tego czy pętla odbiorcza
zatrzymała się przez wykrycie FIN drugiej strony czy przez timeout, a dane były już w
pełni odebrane zanim to nastąpiło. Odnotowane w `TASKS.md` jako obserwacja do pamiętania
przy testowaniu z prawdziwym serwerem HTTP bezpośrednio (bez pośredniczącego mostka
testowego `nc`) — prawdopodobnie zamknie połączenie szybciej i ten log nigdy się nie
pojawi.

Pełny czysty rebuild: **PASS, 0 błędów, 0 nowych ostrzeżeń.** `scripts/test_headless.sh`:
**PASS** po usunięciu tymczasowego kodu testowego z `main.cpp` (ten sam wzorzec co przy
Fazach 6a/6b — HTTP nie jest jeszcze wołane z produkcyjnej ścieżki startowej, dopiero
Faza 7 da mu realnego wywołującego z poziomu aplikacji).

## Napotkane problemy / obserwacje
Opisana wyżej obserwacja o `nc`-moście nieprzekazującym zamknięcia proaktywnie — to
właściwość konkretnego narzędzia testowego użytego do weryfikacji (`nc` jako pośrednik
w `guestfwd`), nie błąd w `HTTP::`/`TCP::`. Zero innych niespodzianek — implementacja
zadziałała poprawnie za pierwszym podejściem, tak jak Faza 6b.

## Co zostało / kolejne kroki
**To zamyka cały zakres sieciowy zlecony na tę samodzielną nocną sesję** (Fazy 4
kamień milowy ICMP → 5 UDP → 6a DHCP → 6b TCP → 6c HTTP/1.0). Zgodnie z instrukcją: nie
ruszałem UI/biblioteki widgetów/package managera — to zostaje na jutro z Kocurowy96.

Naturalny następny krok (Faza 7, syscalle dla aplikacji, żeby userspace mogło faktycznie
używać sieci) świadomie NIE zaczęty — to nowa, osobna decyzja projektowa (kształt API:
styl gniazd czy coś prostszego, jak wygląda pierwsza apka demo), do przegadania przy
najbliższej wspólnej sesji, nie coś do zgadywania samodzielnie teraz.

`TASKS.md` zaktualizowane: Faza 6c przeniesiona do "Zrobione" z pełnym opisem, "Do
zrobienia teraz" wyczyszczone, pozycja o sieci w "Do przegadania" podsumowuje cały
ukończony zakres i wskazuje Fazę 7 jako temat do rozmowy.

## Podsumowanie całej nocnej sesji
Cztery osobne, w pełni zweryfikowane fazy, każda z własnym commitem:
- **Faza 6a (DHCP)** — `ac8d2af`
- **Faza 6b (TCP)** — `86fa284`
- **Faza 6c (HTTP/1.0)** — ten commit

Każda faza zweryfikowana pełnym czystym rebuildem + `test_headless.sh` + realnym testem
funkcjonalnym niezależnie potwierdzonym przez `tcpdump`/logi zewnętrznych narzędzi, nie
tylko własny log kernela. Zero cofniętych/niedokończonych kroków — każda faza zamknięta
zanim ruszyła kolejna.

## Push
Zmiany (`kernel/net/http.h`, `kernel/net/http.cpp`, `CMakeLists.txt`,
`CoworkWithClaude/TASKS.md`, ten raport) do wypchnięcia na `origin/main-0k7z9q` razem z
tym commitem (nie na `main`).
