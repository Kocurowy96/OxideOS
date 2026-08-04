#pragma once
#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE 4096

class PMM {
public:
    static void Init();
    static void* AllocatePage();
    static void* AllocatePages(size_t count);
    static void FreePage(void* ptr);
    static void FreePages(void* ptr, size_t count);

    static uint64_t GetTotalMemory();
    static uint64_t GetFreeMemory();
    static uint64_t GetUsedMemory();

private:
    static void InitBitmap(size_t bitmapSize, void* bufferAddress);
    static void SetFree(void* address, size_t count);
    static void SetUsed(void* address, size_t count);
    
    static bool TestBit(size_t index);
    static void SetBit(size_t index);
    static void ClearBit(size_t index);
};
