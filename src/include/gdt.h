#ifndef GDT_H
#define GDT_H

#include "types.h"

// GDT Entry Structure
struct gdt_entry {
    uint16_t limit_low;      // Lower 16 bits of limit
    uint16_t base_low;       // Lower 16 bits of base
    uint8_t  base_middle;    // Next 8 bits of base
    uint8_t  access;         // Access flags
    uint8_t  granularity;    // Granularity flags + upper 4 bits of limit
    uint8_t  base_high;      // Upper 8 bits of base
} __attribute__((packed));

// GDT Pointer Structure (for LGDT instruction)
struct gdt_pointer {
    uint16_t limit;          // Size of GDT - 1
    uint64_t base;           // Address of first GDT entry
} __attribute__((packed));

// Access byte flags
#define GDT_ACCESS_PRESENT      0x80    // Present bit
#define GDT_ACCESS_PRIV_RING0   0x00    // Ring 0 (kernel)
#define GDT_ACCESS_PRIV_RING3   0x60    // Ring 3 (user)
#define GDT_ACCESS_EXECUTABLE   0x08    // Code segment
#define GDT_ACCESS_RW           0x02    // Readable (code) / Writable (data)
#define GDT_ACCESS_ACCESSED     0x01    // Accessed bit

// Granularity byte flags
#define GDT_GRAN_4K             0x80    // 4KB page granularity
#define GDT_GRAN_BYTE           0x00    // Byte granularity
#define GDT_GRAN_32BIT          0x40    // 32-bit protected mode
#define GDT_GRAN_64BIT          0x20    // 64-bit long mode

// Segment selectors
#define GDT_KERNEL_CODE         0x08    // Offset of kernel code segment
#define GDT_KERNEL_DATA         0x10    // Offset of kernel data segment
#define GDT_USER_CODE           0x18    // Offset of user code segment
#define GDT_USER_DATA           0x20    // Offset of user data segment
#define GDT_TSS                 0x28    // Offset of TSS segment

// Functions
void gdt_init();
void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);
void gdt_set_tss(uint64_t tss_base, uint32_t tss_limit);

// Assembly function to load GDT
extern "C" void gdt_flush(uint64_t gdt_ptr);

#endif
