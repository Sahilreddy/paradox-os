// Task State Segment (TSS) Implementation
#include "../include/tss.h"
#include "../include/serial.h"
#include "../include/memory.h"

// Global TSS
tss_entry tss;

// External function to load TSS
extern "C" void tss_flush(uint16_t tss_selector);

// Initialize TSS
void tss_init() {
    serial_print("TSS: Initializing Task State Segment...\n");
    
    // Clear TSS
    uint8_t* tss_ptr = (uint8_t*)&tss;
    for (uint32_t i = 0; i < sizeof(tss_entry); i++) {
        tss_ptr[i] = 0;
    }
    
    // Set I/O map base to end of TSS (no I/O permissions)
    tss.iomap_base = sizeof(tss_entry);
    
    // Allocate kernel stack for ring 0 (4KB)
    void* kernel_stack = pmm_alloc_page();
    if (!kernel_stack) {
        serial_print("TSS: ERROR - Failed to allocate kernel stack!\n");
        return;
    }
    
    // Set RSP0 (stack grows down, so point to top)
    tss.rsp0 = (uint64_t)kernel_stack + PAGE_SIZE;
    
    serial_print("TSS: RSP0 set to ");
    serial_print_hex(tss.rsp0);
    serial_print("\n");
    
    // TSS is loaded by calling tss_flush with selector 0x28 (GDT entry 5)
    // This will be done after updating GDT
    
    serial_print("TSS: Initialized successfully!\n");
}

// Set kernel stack for current process
void tss_set_kernel_stack(uint64_t stack) {
    tss.rsp0 = stack;
}
