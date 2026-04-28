// Physical Memory Manager - Manages physical RAM pages
#include "../include/memory.h"
#include "../include/serial.h"

// Bitmap to track page allocation (1 bit per page)
static uint8_t* page_bitmap = nullptr;
static uint64_t total_pages = 0;
static uint64_t used_pages = 0;
static uint64_t bitmap_size = 0;

// Memory statistics
static uint64_t total_memory = 0;
static uint64_t usable_memory = 0;

// Kernel end address (defined in linker script)
extern "C" uint64_t _kernel_end;

// Bitmap operations
static inline void bitmap_set(uint64_t bit) {
    page_bitmap[bit / 8] |= (1 << (bit % 8));
}

static inline void bitmap_clear(uint64_t bit) {
    page_bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static inline bool bitmap_test(uint64_t bit) {
    return page_bitmap[bit / 8] & (1 << (bit % 8));
}

// Print hex number
static void print_hex(uint64_t num) {
    serial_print("0x");
    for (int i = 60; i >= 0; i -= 4) {
        uint8_t digit = (num >> i) & 0xF;
        if (digit < 10) {
            serial_putchar('0' + digit);
        } else {
            serial_putchar('A' + digit - 10);
        }
    }
}

// Initialize Physical Memory Manager
void pmm_init(uint64_t mboot_addr) {
    serial_print("PMM: Initializing Physical Memory Manager...\n");
    
    // Find memory map tag
    multiboot2_tag* tag = (multiboot2_tag*)(mboot_addr + 8);
    multiboot2_tag_mmap* mmap_tag = nullptr;
    
    while (tag->type != 0) {
        if (tag->type == 6) { // Memory map tag
            mmap_tag = (multiboot2_tag_mmap*)tag;
            break;
        }
        tag = (multiboot2_tag*)((uint64_t)tag + ((tag->size + 7) & ~7));
    }
    
    if (!mmap_tag) {
        serial_print("PMM: ERROR - No memory map found!\n");
        return;
    }
    
    serial_print("PMM: Memory map found\n");
    
    // Find highest memory address
    uint64_t highest_addr = 0;
    uint32_t entry_count = (mmap_tag->size - 16) / mmap_tag->entry_size;
    
    // Get pointer to first entry (right after the header)
    multiboot2_mmap_entry* entry = (multiboot2_mmap_entry*)((uint8_t*)mmap_tag + 16);
    
    for (uint32_t i = 0; i < entry_count; i++) {
        uint64_t end_addr = entry->base_addr + entry->length;
        
        if (end_addr > highest_addr) {
            highest_addr = end_addr;
        }
        
        total_memory += entry->length;
        if (entry->type == 1) {  // Type 1 = available
            usable_memory += entry->length;
        }
        
        // Move to next entry
        entry = (multiboot2_mmap_entry*)((uint8_t*)entry + mmap_tag->entry_size);
    }
    
    serial_print("PMM: Total memory: ");
    print_hex(total_memory);
    serial_print(" bytes (");
    serial_print_hex((uint32_t)(total_memory / (1024 * 1024)));
    serial_print(" MB)\n");
    
    serial_print("PMM: Usable memory: ");
    print_hex(usable_memory);
    serial_print(" bytes (");
    serial_print_hex((uint32_t)(usable_memory / (1024 * 1024)));
    serial_print(" MB)\n");
    
    // Calculate bitmap size based on highest usable address, not highest address
    total_pages = (highest_addr > usable_memory * 2) ? (usable_memory * 2 / PAGE_SIZE) : (highest_addr / PAGE_SIZE);
    bitmap_size = (total_pages + 7) / 8; // Bytes needed for bitmap
    
    serial_print("PMM: Total pages: ");
    serial_print_hex((uint32_t)total_pages);
    serial_print("\nPMM: Bitmap size: ");
    serial_print_hex((uint32_t)bitmap_size);
    serial_print(" bytes\n");
    
    // Place bitmap after kernel AND after multiboot info
    uint64_t kernel_end = (uint64_t)&_kernel_end;
    uint64_t mboot_end = mboot_addr + 8 + *(uint32_t*)mboot_addr;  // mboot start + size
    uint64_t safe_start = (kernel_end > mboot_end) ? kernel_end : mboot_end;
    page_bitmap = (uint8_t*)((safe_start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
    
    serial_print("PMM: Bitmap placed at: ");
    print_hex((uint64_t)page_bitmap);
    serial_print("\n");
    
    // Initialize bitmap - mark all as used
    for (uint64_t i = 0; i < bitmap_size; i++) {
        page_bitmap[i] = 0xFF;
    }
    used_pages = total_pages;
    
    // Mark free regions in bitmap
    entry = (multiboot2_mmap_entry*)((uint8_t*)mmap_tag + 16);  // Reset to first entry
    
    for (uint32_t i = 0; i < entry_count; i++) {
        if (entry->type == 1) {  // Type 1 = available
            uint64_t start_page = (entry->base_addr + PAGE_SIZE - 1) / PAGE_SIZE;
            uint64_t end_page = (entry->base_addr + entry->length) / PAGE_SIZE;
            
            for (uint64_t page = start_page; page < end_page; page++) {
                if (page < total_pages) {
                    bitmap_clear(page);
                    used_pages--;
                }
            }
        }
        
        // Move to next entry
        entry = (multiboot2_mmap_entry*)((uint8_t*)entry + mmap_tag->entry_size);
    }
    
    // Mark kernel and bitmap as used (be more conservative)
    uint64_t kernel_start_page = 0x100000 / PAGE_SIZE; // 1MB where kernel starts
    uint64_t bitmap_end_page = ((uint64_t)page_bitmap + bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    serial_print("PMM: Kernel pages: 0x");
    serial_print_hex((uint32_t)kernel_start_page);
    serial_print(" to 0x");
    serial_print_hex((uint32_t)bitmap_end_page);
    serial_print("\n");
    
    for (uint64_t page = kernel_start_page; page <= bitmap_end_page; page++) {
        if (page < total_pages && !bitmap_test(page)) {
            bitmap_set(page);
            used_pages++;
        }
    }
    
    serial_print("PMM: Used pages: ");
    serial_print_hex((uint32_t)used_pages);
    serial_print("\nPMM: Free pages: ");
    serial_print_hex((uint32_t)(total_pages - used_pages));
    serial_print("\nPMM: Initialized successfully!\n");
}

// Allocate N physically contiguous pages. Returns the address of the first
// page, or nullptr if no run of N free pages exists. The bitmap is scanned
// linearly and the first run wide enough wins.
void* pmm_alloc_contiguous(uint64_t n_pages) {
    if (n_pages == 0) return nullptr;
    uint64_t run = 0;
    uint64_t run_start = 0;
    for (uint64_t i = 1; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            if (run == 0) run_start = i;
            run++;
            if (run >= n_pages) {
                for (uint64_t j = run_start; j < run_start + n_pages; j++) {
                    bitmap_set(j);
                    used_pages++;
                }
                return (void*)(run_start * PAGE_SIZE);
            }
        } else {
            run = 0;
        }
    }
    serial_print("PMM: ERROR - no contiguous run available\n");
    return nullptr;
}

// Allocate a physical page
void* pmm_alloc_page() {
    // Start from page 1 (skip page 0 to avoid confusing NULL pointers)
    for (uint64_t i = 1; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_pages++;
            void* addr = (void*)(i * PAGE_SIZE);
            return addr;
        }
    }
    
    serial_print("PMM: ERROR - Out of memory! Used: 0x");
    serial_print_hex((uint32_t)used_pages);
    serial_print(" Total: 0x");
    serial_print_hex((uint32_t)total_pages);
    serial_print("\n");
    return nullptr;
}

// Free a physical page
void pmm_free_page(void* page) {
    uint64_t page_index = (uint64_t)page / PAGE_SIZE;
    
    if (page_index >= total_pages) {
        serial_print("PMM: ERROR - Invalid page address!\n");
        return;
    }
    
    if (!bitmap_test(page_index)) {
        serial_print("PMM: WARNING - Double free detected!\n");
        return;
    }
    
    bitmap_clear(page_index);
    used_pages--;
}

// Get memory statistics
uint64_t pmm_get_total_memory() {
    return total_memory;
}

uint64_t pmm_get_used_memory() {
    return used_pages * PAGE_SIZE;
}

uint64_t pmm_get_free_memory() {
    return (total_pages - used_pages) * PAGE_SIZE;
}
