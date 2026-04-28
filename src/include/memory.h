#ifndef MEMORY_H
#define MEMORY_H

#include "types.h"

// Page size (4KB)
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

// Memory region types
#define MEMORY_REGION_FREE 1
#define MEMORY_REGION_RESERVED 2
#define MEMORY_REGION_ACPI_RECLAIMABLE 3
#define MEMORY_REGION_ACPI_NVS 4
#define MEMORY_REGION_BAD 5

// Multiboot2 memory map structures
struct multiboot2_tag {
    uint32_t type;
    uint32_t size;
} __attribute__((packed));

struct multiboot2_mmap_entry {
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
} __attribute__((packed));

struct multiboot2_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    // Don't use flexible array - access via pointer arithmetic
} __attribute__((packed));

// Physical Memory Manager
void pmm_init(uint64_t mboot_addr);
void* pmm_alloc_page();
// Allocate `n` *physically contiguous* pages. Returns nullptr if no run of
// that size is free. Use this for buffers that must be flat in physical
// memory (framebuffer back buffers, DMA buffers, etc.).
void* pmm_alloc_contiguous(uint64_t n_pages);
void pmm_free_page(void* page);
// Bytes in the PMM's bitmap region. This is the *address space* the
// allocator covers, which on x86 includes ROM/MMIO holes and reads as a
// nonsensically large number — useful for debug only, not for display.
uint64_t pmm_get_total_memory();
// RAM the PMM is actually tracking (used + free pages). This is what
// users mean by "total RAM" — display this in UI.
uint64_t pmm_get_managed_memory();
uint64_t pmm_get_used_memory();
uint64_t pmm_get_free_memory();

// Kernel Heap
void heap_init();
void* kmalloc(uint64_t size);
void kfree(void* ptr);
void* krealloc(void* ptr, uint64_t new_size);

// Memory info
void memory_print_info();

#endif // MEMORY_H
