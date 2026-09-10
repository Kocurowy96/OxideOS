#pragma once
#include "isr.h"
#include "../gui/window.h"

class Syscall {
public:
    static void Handler(Registers* regs);
    static void FreeWindowMemory(Window* win); // Wywoływane przez Compositor
};
