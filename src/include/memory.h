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
void pmm_free_page(void* page);
uint64_t pmm_get_total_memory();
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
