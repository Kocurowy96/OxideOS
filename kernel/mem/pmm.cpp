#include "pmm.h"
#include "../limine.h"
#include "../serial.h"

// Define limine requests for memory map and hhdm (Higher Half Direct Map)
__attribute__((used, section(".requests")))
volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

__attribute__((used, section(".requests")))
volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

static uint8_t* bitmap = nullptr;
static size_t bitmap_size = 0; // in bytes
static size_t last_free_index = 0;

static uint64_t total_memory = 0;
static uint64_t free_memory = 0;
static uint64_t used_memory = 0;
static uint64_t highest_address = 0;

uint64_t PMM::GetTotalMemory() { return total_memory; }
uint64_t PMM::GetFreeMemory() { return free_memory; }
uint64_t PMM::GetUsedMemory() { return used_memory; }

bool PMM::TestBit(size_t index) {
    return bitmap[index / 8] & (1 << (index % 8));
}

void PMM::SetBit(size_t index) {
    bitmap[index / 8] |= (1 << (index % 8));
}

void PMM::ClearBit(size_t index) {
    bitmap[index / 8] &= ~(1 << (index % 8));
}

void PMM::SetFree(void* address, size_t count) {
    size_t start_index = (uint64_t)address / PAGE_SIZE;
    for (size_t i = 0; i < count; i++) {
        if (TestBit(start_index + i)) {
            ClearBit(start_index + i);
            free_memory += PAGE_SIZE;
            used_memory -= PAGE_SIZE;
        }
    }
}

void PMM::SetUsed(void* address, size_t count) {
    size_t start_index = (uint64_t)address / PAGE_SIZE;
    for (size_t i = 0; i < count; i++) {
        if (!TestBit(start_index + i)) {
            SetBit(start_index + i);
            free_memory -= PAGE_SIZE;
            used_memory += PAGE_SIZE;
        }
    }
}

void PMM::InitBitmap(size_t bitmapSize, void* bufferAddress) {
    bitmap = (uint8_t*)bufferAddress;
    bitmap_size = bitmapSize;
    // Set all as used initially
    for (size_t i = 0; i < bitmap_size; i++) {
        bitmap[i] = 0xFF;
    }
    // We adjust free memory during SetFree
    free_memory = 0;
    used_memory = total_memory;
}

void PMM::Init() {
    if (memmap_request.response == nullptr || hhdm_request.response == nullptr) {
        SerialPort::WriteString("PMM: Limine requests failed!\n");
        return;
    }

    struct limine_memmap_response* mmap = memmap_request.response;
    
    // Find the highest memory address and total memory
    for (uint64_t i = 0; i < mmap->entry_count; i++) {
        struct limine_memmap_entry* entry = mmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            total_memory += entry->length;
        }
        uint64_t top = entry->base + entry->length;
        if (top > highest_address) {
            highest_address = top;
        }
    }

    // Calculate bitmap size
    bitmap_size = highest_address / PAGE_SIZE / 8;
    if (bitmap_size == 0) bitmap_size = 1;

    // Find a place to put the bitmap
    void* bitmap_addr = nullptr;
    for (uint64_t i = 0; i < mmap->entry_count; i++) {
        struct limine_memmap_entry* entry = mmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= bitmap_size) {
            bitmap_addr = (void*)(entry->base + hhdm_request.response->offset); // Map via HHDM
            break;
        }
    }

    if (!bitmap_addr) {
        SerialPort::WriteString("PMM: Could not find space for bitmap!\n");
        return;
    }

    InitBitmap(bitmap_size, bitmap_addr);

    // Free usable regions
    for (uint64_t i = 0; i < mmap->entry_count; i++) {
        struct limine_memmap_entry* entry = mmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE) {
            SetFree((void*)entry->base, entry->length / PAGE_SIZE);
        }
    }

    // Reserve the bitmap itself
    SetUsed((void*)((uint64_t)bitmap_addr - hhdm_request.response->offset), (bitmap_size / PAGE_SIZE) + 1);
}

void* PMM::AllocatePage() {
    return AllocatePages(1);
}

void* PMM::AllocatePages(size_t count) {
    if (count == 0) return nullptr;

    size_t free_count = 0;
    size_t start_index = 0;

    for (size_t i = last_free_index; i < bitmap_size * 8; i++) {
        if (!TestBit(i)) {
            if (free_count == 0) start_index = i;
            free_count++;
            if (free_count == count) {
                last_free_index = i + 1;
                void* ptr = (void*)(start_index * PAGE_SIZE);
                SetUsed(ptr, count);
                return ptr;
            }
        } else {
            free_count = 0;
        }
    }

    // Wrap around
    free_count = 0;
    for (size_t i = 0; i < last_free_index; i++) {
        if (!TestBit(i)) {
            if (free_count == 0) start_index = i;
            free_count++;
            if (free_count == count) {
                last_free_index = i + 1;
                void* ptr = (void*)(start_index * PAGE_SIZE);
                SetUsed(ptr, count);
                return ptr;
            }
        } else {
            free_count = 0;
        }
    }

    SerialPort::WriteString("PMM: Out of memory!\n");
    return nullptr;
}

void PMM::FreePage(void* ptr) {
    FreePages(ptr, 1);
}

void PMM::FreePages(void* ptr, size_t count) {
    SetFree(ptr, count);
    size_t index = (uint64_t)ptr / PAGE_SIZE;
    if (index < last_free_index) {
        last_free_index = index;
    }
}
