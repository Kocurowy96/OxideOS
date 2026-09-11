#include "critical.h"

volatile int g_critical_depth = 0;

void EnterCritical() {
    asm volatile("cli");
    g_critical_depth++;
}

void ExitCritical() {
    if (g_critical_depth > 0) g_critical_depth--;
    if (g_critical_depth == 0) {
        asm volatile("sti");
    }
}
