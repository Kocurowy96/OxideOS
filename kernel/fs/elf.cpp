#include "elf.h"
#include "../mem/pmm.h"
#include "../mem/vmm.h"
#include "../limine.h"
#include "../serial.h"

extern volatile struct limine_hhdm_request hhdm_request;

uint64_t ELF::Load(uint8_t* file_data) {
    ELF64_Ehdr* header = (ELF64_Ehdr*)file_data;
    if (header->e_ident[0] != 0x7F || header->e_ident[1] != 'E' || 
        header->e_ident[2] != 'L' || header->e_ident[3] != 'F') {
        SerialPort::WriteString("ELF: Invalid magic!\n");
        return 0;
    }

    if (header->e_machine != 0x3E) { // EM_X86_64
        SerialPort::WriteString("ELF: Not an x86_64 executable!\n");
        return 0;
    }

    ELF64_Phdr* phdrs = (ELF64_Phdr*)(file_data + header->e_phoff);
    for (int i = 0; i < header->e_phnum; i++) {
        if (phdrs[i].p_type == 1) { // PT_LOAD
            uint64_t mem_size = phdrs[i].p_memsz;
            uint64_t file_size = phdrs[i].p_filesz;
            uint64_t vaddr = phdrs[i].p_vaddr;
            uint64_t offset = phdrs[i].p_offset;

            if (mem_size == 0) continue;

            uint64_t page_count = (mem_size + (vaddr & 0xFFF) + 0xFFF) / 4096;
            if (page_count == 0) page_count = 1;
            
            // Allocate physical memory
            void* phys_ptr = PMM::AllocatePages(page_count);
            if (!phys_ptr) {
                SerialPort::WriteString("ELF: Out of memory!\n");
                return 0;
            }
            
            uint8_t* dest = (uint8_t*)((uint64_t)phys_ptr + hhdm_request.response->offset);
            
            // Zero memory (BSS)
            for (uint64_t j = 0; j < page_count * 4096; j++) dest[j] = 0;
            
            // Copy data, handle alignment
            uint64_t page_offset = vaddr & 0xFFF;
            for (uint64_t j = 0; j < file_size; j++) {
                dest[page_offset + j] = file_data[offset + j];
            }
            
            // Map pages
            uint64_t aligned_vaddr = vaddr & ~0xFFF;
            for (uint64_t j = 0; j < page_count; j++) {
                VMM::MapPage((uint64_t)phys_ptr + j * 4096, aligned_vaddr + j * 4096, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
            }
        }
    }
    
    return header->e_entry;
}
