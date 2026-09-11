#pragma once

// Prowizoryczna (ale zagniezdzalna) ochrona krytycznych sekcji dla dzielonego stanu
// jadra (bitmapa PMM, cache FAT itp.), do ktorego moga dobijac sie rownolegle
// preemptable taski przez syscalle. To NIE jest prawdziwy lock (dziala tylko na
// pojedynczym rdzeniu, blokujac przerwania) - ale wystarcza, zeby dwa taski nie
// nadepnely sobie na te same dane w trakcie testowania. Docelowo do zastapienia
// porzadnymi blokadami przy wiekszym przepisywaniu (np. ext4).
//
// Liczymy glebokosc zagniezdzenia (jedna, wspolna dla calego jadra zmienna -
// stad extern + definicja w critical.cpp), zeby wewnetrzne EnterCritical/
// ExitCritical (np. ATA wolane z wnetrza FAT32) nie wlaczaly przerwan
// przedwczesnie i nie psuly ochrony sekcji nadrzednej.

extern volatile int g_critical_depth;

void EnterCritical();
void ExitCritical();
