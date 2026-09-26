#pragma once
#include <stdint.h>

// Faza 3+ (patrz CoworkWithClaude/PLAN_networking.md): adresy IP zaszyte na sztywno
// (decyzja "statyczne IP" - dopasowane do domyslnej podsieci QEMU user-mode/SLIRP).
// Reprezentowane jako 4 surowe bajty w kolejnosci sieciowej (tej samej co w naglowkach
// ARP/IP) zamiast uint32_t - unika kwestii endianness konwersji dla tak prostego, na
// sztywno zaszytego przypadku.
static const uint8_t NET_OUR_IP[4] = {10, 0, 2, 15};
static const uint8_t NET_GATEWAY_IP[4] = {10, 0, 2, 2};
