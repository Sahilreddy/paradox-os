// Virtual Memory Manager and 4-Level Paging
#include "../include/paging.h"
#include "../include/memory.h"
#include "../include/serial.h"

// Current PML4 (top-level page table)
static page_table_t* current_pml4 = nullptr;

// Extract page table indices from virtual address
#define PML4_INDEX(addr) (((addr) >> 39) & 0x1FF)
#define PDPT_INDEX(addr) (((addr) >> 30) & 0x1FF)
#define PD_INDEX(addr)   (((addr) >> 21) & 0x1FF)
#define PT_INDEX(addr)   (((addr) >> 12) & 0x1FF)

// Get physical address from page table entry
#define PTE_ADDR(pte) ((pte) & 0x000FFFFFFFFFF000UL)

// Temporary mapping area for accessing page tables
#define TEMP_MAP_ADDR 0xFFFFFF8000000000UL

// Allocate and zero a page table
static page_table_t* alloc_page_table() {
    void* page = pmm_alloc_page();
    if (!page) {
        serial_print("Paging: ERROR - Failed to allocate page table!\n");
        return nullptr;
    }
    
    // Zero out the page table
    page_table_t* table = (page_table_t*)page;
    for (int i = 0; i < 512; i++) {
        (*table)[i] = 0;
    }
    
    return table;
}

// Get or create a page table entry
static uint64_t* get_or_create_pte(uint64_t virt, bool create) {
    uint64_t pml4_idx = PML4_INDEX(virt);
    uint64_t pdpt_idx = PDPT_INDEX(virt);
    uint64_t pd_idx = PD_INDEX(virt);
    uint64_t pt_idx = PT_INDEX(virt);
    
    // Get PML4 entry
    uint64_t pml4e = (*current_pml4)[pml4_idx];
    page_table_t* pdpt;
    
    if (!(pml4e & PAGE_PRESENT)) {
        if (!create) return nullptr;
        pdpt = alloc_page_table();
        if (!pdpt) return nullptr;
        (*current_pml4)[pml4_idx] = (uint64_t)pdpt | PAGE_PRESENT | PAGE_WRITE;
    } else {
        pdpt = (page_table_t*)PTE_ADDR(pml4e);
    }
    
    // Get PDPT entry
    uint64_t pdpte = (*pdpt)[pdpt_idx];
    page_table_t* pd;
    
    if (!(pdpte & PAGE_PRESENT)) {
        if (!create) return nullptr;
        pd = alloc_page_table();
        if (!pd) return nullptr;
        (*pdpt)[pdpt_idx] = (uint64_t)pd | PAGE_PRESENT | PAGE_WRITE;
    } else {
        pd = (page_table_t*)PTE_ADDR(pdpte);
    }
    
    // Get PD entry
    uint64_t pde = (*pd)[pd_idx];
    page_table_t* pt;
    
    if (!(pde & PAGE_PRESENT)) {
        if (!create) return nullptr;
        pt = alloc_page_table();
        if (!pt) return nullptr;
        (*pd)[pd_idx] = (uint64_t)pt | PAGE_PRESENT | PAGE_WRITE;
    } else {
        pt = (page_table_t*)PTE_ADDR(pde);
    }
    
    // Return pointer to PT entry
    return &(*pt)[pt_idx];
}

// Initialize paging
void paging_init() {
    serial_print("Paging: Initializing 4-level paging...\n");
    
    // Get current CR3 (PML4 from bootloader)
    asm volatile("mov %%cr3, %0" : "=r"(current_pml4));
    
    serial_print("Paging: Using bootloader PML4 at 0x");
    serial_print_hex((uint32_t)(uint64_t)current_pml4);
    serial_print("\n");
    
    // We're already in long mode with paging enabled by boot.asm
    // The bootloader has set up identity mapping for us
    // We'll continue using it for now
    
    serial_print("Paging: 4-level paging active (using bootloader tables)!\n");
}

// Map virtual to physical address
bool paging_map(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t* pte = get_or_create_pte(virt, true);
    if (!pte) {
        return false;
    }
    
    *pte = (phys & 0x000FFFFFFFFFF000UL) | flags;
    
    // Invalidate TLB entry
    asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
    
    return true;
}

// Unmap virtual address
void paging_unmap(uint64_t virt) {
    uint64_t* pte = get_or_create_pte(virt, false);
    if (!pte) return;
    
    *pte = 0;
    
    // Invalidate TLB entry
    asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

// Get physical address from virtual
uint64_t paging_get_physical(uint64_t virt) {
    uint64_t* pte = get_or_create_pte(virt, false);
    if (!pte || !(*pte & PAGE_PRESENT)) {
        return 0;
    }
    
    return PTE_ADDR(*pte) | (virt & 0xFFF);
}

// Identity map a region
void paging_identity_map_region(uint64_t start, uint64_t size, uint64_t flags) {
    uint64_t addr = start & ~0xFFFUL; // Align to page
    uint64_t end = (start + size + 0xFFF) & ~0xFFFUL;
    
    while (addr < end) {
        paging_map(addr, addr, flags);
        addr += PAGE_SIZE;
    }
}
