#pragma once
#include <stdint.h>
#include "../cpu/isr.h"

class OSOD {
public:
    static void Draw(Registers* regs);
};
