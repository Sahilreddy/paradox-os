#include "../include/gdt.h"
#include "../include/serial.h"
#include "../include/tss.h"

// GDT with 7 entries: null, kernel code, kernel data, user code, user data, TSS low, TSS high
#define GDT_ENTRIES 7
static gdt_entry gdt[GDT_ENTRIES];
static gdt_pointer gdt_ptr;

// Set a GDT entry
void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    
    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = (limit >> 16) & 0x0F;
    gdt[num].granularity |= gran & 0xF0;
    
    gdt[num].access = access;
}

// Initialize the GDT
void gdt_init() {
    gdt_ptr.limit = (sizeof(gdt_entry) * GDT_ENTRIES) - 1;
    gdt_ptr.base = (uint64_t)&gdt;
    
    // Null descriptor (required)
    gdt_set_gate(0, 0, 0, 0, 0);
    
    // Kernel code segment (ring 0)
    // Base = 0, Limit = 0xFFFFF (full 4GB with granularity)
    // Access: Present | Ring 0 | Executable | Readable
    // Gran: 64-bit | 4KB pages
    gdt_set_gate(1, 0, 0xFFFFFFFF, 
                 GDT_ACCESS_PRESENT | GDT_ACCESS_PRIV_RING0 | GDT_ACCESS_EXECUTABLE | GDT_ACCESS_RW,
                 GDT_GRAN_64BIT | GDT_GRAN_4K);
    
    // Kernel data segment (ring 0)
    // Base = 0, Limit = 0xFFFFF
    // Access: Present | Ring 0 | Writable
    // Gran: 64-bit | 4KB pages
    gdt_set_gate(2, 0, 0xFFFFFFFF,
                 GDT_ACCESS_PRESENT | GDT_ACCESS_PRIV_RING0 | GDT_ACCESS_RW,
                 GDT_GRAN_64BIT | GDT_GRAN_4K);
    
    // User code segment (ring 3) - for future use
    gdt_set_gate(3, 0, 0xFFFFFFFF,
                 GDT_ACCESS_PRESENT | GDT_ACCESS_PRIV_RING3 | GDT_ACCESS_EXECUTABLE | GDT_ACCESS_RW,
                 GDT_GRAN_64BIT | GDT_GRAN_4K);
    
    // User data segment (ring 3) - for future use
    gdt_set_gate(4, 0, 0xFFFFFFFF,
                 GDT_ACCESS_PRESENT | GDT_ACCESS_PRIV_RING3 | GDT_ACCESS_RW,
                 GDT_GRAN_64BIT | GDT_GRAN_4K);
    
    // TSS segment (takes 2 entries in x86_64)
    // Will be set up by tss_init()
    gdt_set_gate(5, 0, 0, 0, 0);  // TSS low
    gdt_set_gate(6, 0, 0, 0, 0);  // TSS high
    
    // Load the new GDT
    gdt_flush((uint64_t)&gdt_ptr);
    
    serial_print("GDT: Initialized with 7 segments (including TSS)\n");
}

// Set TSS descriptor in GDT
void gdt_set_tss(uint64_t tss_base, uint32_t tss_limit) {
    // TSS descriptor is 16 bytes in x86_64 (uses 2 GDT entries)
    // Entry 5: TSS low 8 bytes
    // Entry 6: TSS high 8 bytes
    
    gdt[5].limit_low = tss_limit & 0xFFFF;
    gdt[5].base_low = tss_base & 0xFFFF;
    gdt[5].base_middle = (tss_base >> 16) & 0xFF;
    gdt[5].access = 0x89;  // Present, Ring 0, TSS available
    gdt[5].granularity = ((tss_limit >> 16) & 0x0F);
    gdt[5].base_high = (tss_base >> 24) & 0xFF;
    
    // High 8 bytes of TSS descriptor
    gdt[6].limit_low = (tss_base >> 32) & 0xFFFF;
    gdt[6].base_low = (tss_base >> 48) & 0xFFFF;
    gdt[6].base_middle = 0;
    gdt[6].access = 0;
    gdt[6].granularity = 0;
    gdt[6].base_high = 0;
    
    // Reload GDT to pick up TSS changes
    gdt_flush((uint64_t)&gdt_ptr);
    
    serial_print("GDT: TSS descriptor set at selector 0x28\n");
}
