#include "../include/pic.h"
#include "../include/serial.h"

// I/O port operations
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Small delay for PIC operations
static void io_wait() {
    outb(0x80, 0);  // Port 0x80 is used for POST codes
}

// Remap PIC IRQs to avoid conflict with CPU exceptions
// By default, IRQ0-7 are mapped to INT 0x08-0x0F (conflicts with CPU exceptions)
// We remap to INT 0x20-0x2F (32-47)
void pic_init() {
    uint8_t mask1, mask2;
    
    // Save current masks
    mask1 = inb(PIC1_DATA);
    mask2 = inb(PIC2_DATA);
    
    // Start initialization sequence (ICW1)
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    
    // ICW2: Set vector offsets
    outb(PIC1_DATA, 0x20);  // Master PIC offset (IRQ0-7 -> INT 0x20-0x27)
    io_wait();
    outb(PIC2_DATA, 0x28);  // Slave PIC offset (IRQ8-15 -> INT 0x28-0x2F)
    io_wait();
    
    // ICW3: Tell master about slave at IRQ2
    outb(PIC1_DATA, 0x04);  // Slave is at IRQ2 (binary 00000100)
    io_wait();
    outb(PIC2_DATA, 0x02);  // Slave cascade identity (binary 00000010)
    io_wait();
    
    // ICW4: Set 8086 mode
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();
    
    // Mask all IRQs initially (we'll enable specific ones later)
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
    
    serial_print("PIC: Initialized and remapped to INT 0x20-0x2F\n");
}

// Send End Of Interrupt signal
void pic_send_eoi(uint8_t irq) {
    // If IRQ came from slave PIC, send EOI to both
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    // Always send EOI to master
    outb(PIC1_COMMAND, PIC_EOI);
}

// Disable all IRQs
void pic_disable_all() {
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

// Mask (disable) a specific IRQ
void pic_set_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    
    value = inb(port) | (1 << irq);
    outb(port, value);
}

// Unmask (enable) a specific IRQ
void pic_clear_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    
    value = inb(port) & ~(1 << irq);
    outb(port, value);
}
