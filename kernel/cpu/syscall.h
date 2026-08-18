#pragma once
#include "isr.h"

class Syscall {
public:
    static void Handler(Registers* regs);
};
