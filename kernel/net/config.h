#pragma once
#include <stdint.h>

// Faza 6a (patrz CoworkWithClaude/PLAN_networking.md): NIE sa juz zaszytymi na sztywno
// stalymi - DHCP::Run() (wolane raz na starcie z kernel/main.cpp, PRZED czymkolwiek innym
// w tym pliku) wypelnia je prawdziwymi wartosciami z serwera DHCP (yiaddr z ACK + opcja 3
// "Router"). Do tego momentu obie zostaja {0,0,0,0} - jesli jakis kod uzyje ich wczesniej
// (nie powinien), dostanie bezuzyteczny adres zamiast przypadkowo-poprawnego, jak przy
// starym hardkodowaniu 10.0.2.15/10.0.2.2. Definicje w config.cpp (musza byc w jednym
// miejscu, bo sa teraz mutowalne, nie `static const` kopiowane do kazdej jednostki
// kompilacji jak wczesniej).
extern uint8_t NET_OUR_IP[4];
extern uint8_t NET_GATEWAY_IP[4];
