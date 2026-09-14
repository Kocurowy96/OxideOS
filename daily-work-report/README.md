# daily-work-report

Lekkie raporty z każdego uruchomienia agenta w chmurze (pon-pt, gałąź `daily-work`) —
żeby nie trzeba było grzebać w logach sesji przez panel, żeby wiedzieć co się stało.

- Jeden plik na uruchomienie: `summary-DD-MM-RR-<timestamp>.md` (np. `summary-15-09-26-0900.md`)
- Szablon: [`TEMPLATE.md`](TEMPLATE.md)
- Pliki żyją na gałęzi `daily-work` razem z resztą zmian z danego dnia — widoczne dopiero
  po `git fetch && git checkout daily-work`, tak jak reszta pracy agenta.
