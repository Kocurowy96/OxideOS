#include "vmm.h"
#include "pmm.h"
#include "../limine.h"
#include "../serial.h"

extern volatile struct limine_hhdm_request hhdm_request;

static uint64_t* kernel_pml4 = nullptr;

static void invlpg(uint64_t addr) {
    asm volatile("invlpg (%0)" ::"r" (addr) : "memory");
}

uint64_t* VMM::GetNextLevel(uint64_t* currentLevel, uint16_t index, bool allocate) {
    uint64_t entry = currentLevel[index];
    if (entry & PAGE_PRESENT) {
        uint64_t phys = entry & ~(0xFFF);
        return (uint64_t*)(phys + hhdm_request.response->offset);
    }

    if (allocate) {
        void* newPage = PMM::AllocatePage();
        if (!newPage) return nullptr;
        
        uint64_t* virtPage = (uint64_t*)((uint64_t)newPage + hhdm_request.response->offset);
        for (int i = 0; i < 512; i++) virtPage[i] = 0;

        currentLevel[index] = (uint64_t)newPage | PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER;
        return virtPage;
    }

    return nullptr;
}

void VMM::MapPage(uint64_t physAddr, uint64_t virtAddr, uint64_t flags) {
    uint16_t pml4_index = (virtAddr >> 39) & 0x1FF;
    uint16_t pdpt_index = (virtAddr >> 30) & 0x1FF;
    uint16_t pd_index   = (virtAddr >> 21) & 0x1FF;
    uint16_t pt_index   = (virtAddr >> 12) & 0x1FF;

    uint64_t* pdpt = GetNextLevel(kernel_pml4, pml4_index, true);
    uint64_t* pd = GetNextLevel(pdpt, pdpt_index, true);
    uint64_t* pt = GetNextLevel(pd, pd_index, true);

    if (pt) {
        pt[pt_index] = (physAddr & ~(0xFFF)) | flags;
        invlpg(virtAddr);
    }
}

void VMM::SwitchPML4(uint64_t pml4_phys) {
    asm volatile("mov %0, %%cr3" :: "r"(pml4_phys) : "memory");
}

void VMM::Init() {
    // Odczytaj aktualny CR3 (PML4 Limine)
    uint64_t current_cr3;
    asm volatile("mov %%cr3, %0" : "=r"(current_cr3));
    uint64_t hhdm_offset = hhdm_request.response->offset;
    
    uint64_t* current_pml4 = (uint64_t*)(current_cr3 + hhdm_offset);
    
    // Alokuj nasz nowy PML4
    kernel_pml4 = (uint64_t*)((uint64_t)PMM::AllocatePage() + hhdm_offset);
    
    // Wyzeruj dolną połowę (przestrzeń użytkownika)
    for (int i = 0; i < 256; i++) {
        kernel_pml4[i] = 0;
    }
    
    // Skopiuj wyższą połowę (przestrzeń jądra, HHDM, Framebuffer) od Limine
    for (int i = 256; i < 512; i++) {
        kernel_pml4[i] = current_pml4[i];
    }

    uint64_t pml4_phys = (uint64_t)kernel_pml4 - hhdm_offset;
    SwitchPML4(pml4_phys);
}
