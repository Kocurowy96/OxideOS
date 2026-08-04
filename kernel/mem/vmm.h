#pragma once
#include <stdint.h>
#include <stddef.h>

#define PAGE_PRESENT    (1 << 0)
#define PAGE_WRITABLE   (1 << 1)
#define PAGE_USER       (1 << 2)
#define PAGE_LARGE      (1 << 7)

class VMM {
public:
    static void Init();
    static void MapPage(uint64_t physAddr, uint64_t virtAddr, uint64_t flags);
    static void SwitchPML4(uint64_t pml4_phys);
private:
    static uint64_t* GetNextLevel(uint64_t* currentLevel, uint16_t index, bool allocate);
};
