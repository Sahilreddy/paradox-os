#ifndef PAGING_H
#define PAGING_H

#include "types.h"

// Page table entry flags
#define PAGE_PRESENT    (1UL << 0)
#define PAGE_WRITE      (1UL << 1)
#define PAGE_USER       (1UL << 2)
#define PAGE_WRITETHROUGH (1UL << 3)
#define PAGE_NOCACHE    (1UL << 4)
#define PAGE_ACCESSED   (1UL << 5)
#define PAGE_DIRTY      (1UL << 6)
#define PAGE_HUGE       (1UL << 7)
#define PAGE_GLOBAL     (1UL << 8)
#define PAGE_NX         (1UL << 63)

// Page table structure (512 entries of 8 bytes = 4KB)
typedef uint64_t page_table_t[512];

// Initialize paging system
void paging_init();

// Map a virtual address to physical address
bool paging_map(uint64_t virt, uint64_t phys, uint64_t flags);

// Unmap a virtual address
void paging_unmap(uint64_t virt);

// Get physical address from virtual
uint64_t paging_get_physical(uint64_t virt);

// Identity map a region (virt == phys)
void paging_identity_map_region(uint64_t start, uint64_t size, uint64_t flags);

#endif // PAGING_H
