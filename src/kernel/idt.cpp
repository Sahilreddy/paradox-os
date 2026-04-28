#include "../include/idt.h"
#include "../include/serial.h"
#include "../include/vga.h"
#include "../include/pic.h"
#include "../include/keyboard.h"
#include "../include/timer.h"
#include "../include/syscall.h"
#include "../include/mouse.h"

// IDT with 256 entries (0-255)
#define IDT_ENTRIES 256
static idt_entry idt[IDT_ENTRIES];
static idt_pointer idt_ptr;

// Set an IDT entry
void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector, uint8_t type_attr) {
    idt[num].offset_low = handler & 0xFFFF;
    idt[num].offset_mid = (handler >> 16) & 0xFFFF;
    idt[num].offset_high = (handler >> 32) & 0xFFFFFFFF;
    
    idt[num].selector = selector;
    idt[num].ist = 0;  // No IST for now
    idt[num].type_attr = type_attr;
    idt[num].zero = 0;
}

// Assembly function to load IDT
extern "C" void idt_flush(uint64_t idt_ptr);

// Initialize the IDT
void idt_init() {
    idt_ptr.limit = (sizeof(idt_entry) * IDT_ENTRIES) - 1;
    idt_ptr.base = (uint64_t)&idt;
    
    // Clear IDT
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, 0, 0, 0);
    }
    
    // Set up CPU exception handlers (0-31)
    // Selector 0x08 = kernel code segment
    idt_set_gate(0,  (uint64_t)isr0,  0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(1,  (uint64_t)isr1,  0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(2,  (uint64_t)isr2,  0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(3,  (uint64_t)isr3,  0x08, IDT_TYPE_TRAP);      // Breakpoint is trap
    idt_set_gate(4,  (uint64_t)isr4,  0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(5,  (uint64_t)isr5,  0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(6,  (uint64_t)isr6,  0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(7,  (uint64_t)isr7,  0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(8,  (uint64_t)isr8,  0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(10, (uint64_t)isr10, 0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(11, (uint64_t)isr11, 0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(12, (uint64_t)isr12, 0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(13, (uint64_t)isr13, 0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(14, (uint64_t)isr14, 0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(16, (uint64_t)isr16, 0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(17, (uint64_t)isr17, 0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(18, (uint64_t)isr18, 0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(19, (uint64_t)isr19, 0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(20, (uint64_t)isr20, 0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(30, (uint64_t)isr30, 0x08, IDT_TYPE_INTERRUPT);
    
    // Set up IRQ handlers (32-47)
    idt_set_gate(32, (uint64_t)irq0,  0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(33, (uint64_t)irq1,  0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(34, (uint64_t)irq2,  0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(35, (uint64_t)irq3,  0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(36, (uint64_t)irq4,  0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(37, (uint64_t)irq5,  0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(38, (uint64_t)irq6,  0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(39, (uint64_t)irq7,  0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(40, (uint64_t)irq8,  0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(41, (uint64_t)irq9,  0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(42, (uint64_t)irq10, 0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(43, (uint64_t)irq11, 0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(44, (uint64_t)irq12, 0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(45, (uint64_t)irq13, 0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(46, (uint64_t)irq14, 0x08, IDT_TYPE_INTERRUPT);
    idt_set_gate(47, (uint64_t)irq15, 0x08, IDT_TYPE_INTERRUPT);
    
    // Load the IDT
    idt_flush((uint64_t)&idt_ptr);
    
    serial_print("IDT: Initialized with exception and IRQ handlers\n");
}

// Exception names for debugging
static const char* exception_messages[] = {
    "Divide By Zero",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 FPU Error",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating Point Exception",
    "Virtualization Exception"
};

// Common exception handler
extern "C" void exception_handler(uint64_t vector, uint64_t error_code, interrupt_frame* frame) {
    // Page fault handler (exception 14)
    if (vector == 14) {
        uint64_t cr2;
        asm volatile("mov %%cr2, %0" : "=r"(cr2));
        
        serial_print("\n=== PAGE FAULT ===\n");
        serial_print("Address: 0x");
        serial_print_hex((uint32_t)(cr2 >> 32));
        serial_print_hex((uint32_t)cr2);
        serial_print("\nError: ");
        
        if (error_code & 0x01) serial_print("PRESENT ");
        else serial_print("NOT-PRESENT ");
        if (error_code & 0x02) serial_print("WRITE ");
        else serial_print("READ ");
        if (error_code & 0x04) serial_print("USER ");
        else serial_print("KERNEL ");
        if (error_code & 0x10) serial_print("INSTRUCTION-FETCH");
        serial_print("\n");
        
        vga_setcolor(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_print("\n!!! PAGE FAULT !!!\n");
        vga_print("Faulting address: 0x");
        vga_print_hex((uint32_t)(cr2 >> 32));
        vga_print_hex((uint32_t)cr2);
        vga_print("\n");
    }
    
    // General exception display
    // Print to both serial and VGA
    vga_setcolor(VGA_COLOR_WHITE, VGA_COLOR_RED);
    vga_print("\n\n!!! KERNEL PANIC - CPU EXCEPTION !!!\n\n");
    
    vga_setcolor(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_print("Exception: ");
    
    if (vector < 21) {
        vga_print(exception_messages[vector]);
    } else if (vector == 30) {
        vga_print("Security Exception");
    } else {
        vga_print("Unknown Exception");
    }
    
    vga_print(" (Vector ");
    vga_print_hex((uint32_t)vector);
    vga_print(")\n");
    
    vga_setcolor(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_print("Error Code: ");
    vga_print_hex((uint32_t)error_code);
    vga_print("\n");
    
    vga_print("RIP: ");
    vga_print_hex((uint32_t)(frame->rip >> 32));
    vga_print_hex((uint32_t)frame->rip);
    vga_print("\n");
    
    vga_print("RSP: ");
    vga_print_hex((uint32_t)(frame->rsp >> 32));
    vga_print_hex((uint32_t)frame->rsp);
    vga_print("\n");
    
    vga_print("RFLAGS: ");
    vga_print_hex((uint32_t)(frame->rflags >> 32));
    vga_print_hex((uint32_t)frame->rflags);
    vga_print("\n");
    
    // Print to serial as well
    serial_print("\n!!! KERNEL PANIC !!!\n");
    serial_print("Exception: ");
    if (vector < 21) {
        serial_print(exception_messages[vector]);
    }
    serial_print("\n");
    
    // Halt the system
    vga_setcolor(VGA_COLOR_RED, VGA_COLOR_BLACK);
    vga_print("\nSystem halted.\n");
    
    while (1) {
        asm volatile("hlt");
    }
}

// IRQ handler - dispatches to specific device handlers
extern "C" void irq_handler(uint64_t irq_num) {
    // Dispatch to specific handler
    switch (irq_num) {
        case 32:  // IRQ0 - Timer
            timer_handler();
            pic_send_eoi(0);
            break;
        case 33:  // IRQ1 - Keyboard
            keyboard_handler();
            break;
        case 34:  // IRQ2 - Cascade (never raised)
            break;
        case 44:  // IRQ12 - PS/2 Mouse
            mouse_handler();
            break;
        default:
            // Unknown IRQ
            serial_print("IRQ: Unhandled IRQ ");
            serial_print_hex((uint32_t)irq_num);
            serial_print("\n");
            pic_send_eoi(irq_num - 32);
            break;
    }
    // Note: keyboard_handler() sends EOI itself
}
