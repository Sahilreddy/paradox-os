#ifndef TSS_H
#define TSS_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Task State Segment structure (x86_64)
struct tss_entry {
    uint32_t reserved0;
    uint64_t rsp0;      // Stack pointer for ring 0
    uint64_t rsp1;      // Stack pointer for ring 1
    uint64_t rsp2;      // Stack pointer for ring 2
    uint64_t reserved1;
    uint64_t ist[7];    // Interrupt stack table
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

// Initialize TSS
void tss_init();

// Set kernel stack for ring 0
void tss_set_kernel_stack(uint64_t stack);

// Load TSS into Task Register
extern "C" void tss_flush(uint16_t tss_selector);

// Get TSS entry for GDT
extern tss_entry tss;

#ifdef __cplusplus
}
#endif

#endif // TSS_H
