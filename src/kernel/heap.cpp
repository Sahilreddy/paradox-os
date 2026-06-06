// Kernel Heap - Simple first-fit allocator
#include "../include/memory.h"
#include "../include/serial.h"

// Heap block header
struct heap_block {
    uint64_t size;      // Size of block (including header)
    bool is_free;       // Is this block free?
    heap_block* next;   // Next block in list
} __attribute__((packed));

static heap_block* heap_start = nullptr;
static uint64_t heap_size = 0;
// 16 MiB. Big enough for a 1024x768x32 back buffer (~3 MiB) plus window
// state, with headroom. Bump again if we add bigger off-screen surfaces.
static const uint64_t INITIAL_HEAP_PAGES = 4096;

// Initialize kernel heap.
//
// We need a flat run of physical pages so that pointer arithmetic across
// the whole heap is valid. Earlier this code called pmm_alloc_page() in a
// loop, but PMM skips reserved regions (BIOS/VGA holes around 0xA0000),
// so the run was not actually contiguous and the resulting heap silently
// indexed into ROM memory once it grew beyond ~640 KiB. Use the proper
// contiguous allocator and trust it.
void heap_init() {
    serial_print("Heap: Initializing kernel heap...\n");

    void* base = pmm_alloc_contiguous(INITIAL_HEAP_PAGES);
    if (!base) {
        // Fall back: maybe we asked for too much. Halve repeatedly until
        // something fits, so the heap is always *some* valid size.
        for (uint64_t n = INITIAL_HEAP_PAGES / 2; n >= 16; n /= 2) {
            base = pmm_alloc_contiguous(n);
            if (base) {
                heap_size = n * PAGE_SIZE;
                break;
            }
        }
        if (!base) {
            serial_print("Heap: ERROR - no contiguous run available\n");
            return;
        }
    } else {
        heap_size = INITIAL_HEAP_PAGES * PAGE_SIZE;
    }
    heap_start = (heap_block*)base;
    
    // Initialize first block
    heap_start->size = heap_size - sizeof(heap_block);
    heap_start->is_free = true;
    heap_start->next = nullptr;
    
    serial_print("Heap: Initialized with ");
    serial_print_hex((uint32_t)(heap_size / 1024));
    serial_print(" KB at 0x");
    serial_print_hex((uint32_t)(uint64_t)heap_start);
    serial_print("\n");
}

// Allocate memory from heap
void* kmalloc(uint64_t size) {
    if (size == 0) return nullptr;
    
    // Align size to 8 bytes
    size = (size + 7) & ~7;
    
    // Find first fit
    heap_block* current = heap_start;
    while (current) {
        if (current->is_free && current->size >= size) {
            // Found suitable block
            if (current->size >= size + sizeof(heap_block) + 8) {
                // Split block
                heap_block* new_block = (heap_block*)((uint64_t)current + sizeof(heap_block) + size);
                new_block->size = current->size - size - sizeof(heap_block);
                new_block->is_free = true;
                new_block->next = current->next;
                
                current->size = size;
                current->next = new_block;
            }
            
            current->is_free = false;
            return (void*)((uint64_t)current + sizeof(heap_block));
        }
        current = current->next;
    }
    
    serial_print("Heap: ERROR - Out of memory for allocation of ");
    serial_print_hex((uint32_t)size);
    serial_print(" bytes\n");
    return nullptr;
}

// Free memory
void kfree(void* ptr) {
    if (!ptr) return;
    
    heap_block* block = (heap_block*)((uint64_t)ptr - sizeof(heap_block));
    block->is_free = true;
    
    // Coalesce with next block if free
    if (block->next && block->next->is_free) {
        block->size += sizeof(heap_block) + block->next->size;
        block->next = block->next->next;
    }
    
    // Coalesce with previous block if free
    heap_block* current = heap_start;
    while (current && current->next != block) {
        current = current->next;
    }
    
    if (current && current->is_free) {
        current->size += sizeof(heap_block) + block->size;
        current->next = block->next;
    }
}

// Reallocate memory
void* krealloc(void* ptr, uint64_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) {
        kfree(ptr);
        return nullptr;
    }
    
    heap_block* block = (heap_block*)((uint64_t)ptr - sizeof(heap_block));
    
    // If new size fits in current block, just return it
    if (block->size >= new_size) {
        return ptr;
    }
    
    // Allocate new block and copy
    void* new_ptr = kmalloc(new_size);
    if (!new_ptr) return nullptr;
    
    // Copy old data
    uint8_t* src = (uint8_t*)ptr;
    uint8_t* dst = (uint8_t*)new_ptr;
    for (uint64_t i = 0; i < block->size; i++) {
        dst[i] = src[i];
    }
    
    kfree(ptr);
    return new_ptr;
}
