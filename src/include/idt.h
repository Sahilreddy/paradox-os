#ifndef IDT_H
#define IDT_H

#include "types.h"

// IDT Entry Structure (64-bit)
struct idt_entry {
    uint16_t offset_low;     // Lower 16 bits of handler address
    uint16_t selector;       // Kernel code segment selector
    uint8_t  ist;            // Interrupt Stack Table offset (0 for now)
    uint8_t  type_attr;      // Type and attributes
    uint16_t offset_mid;     // Middle 16 bits of handler address
    uint32_t offset_high;    // Upper 32 bits of handler address
    uint32_t zero;           // Reserved, must be zero
} __attribute__((packed));

// IDT Pointer Structure (for LIDT instruction)
struct idt_pointer {
    uint16_t limit;          // Size of IDT - 1
    uint64_t base;           // Address of first IDT entry
} __attribute__((packed));

// Type and attribute flags
#define IDT_TYPE_INTERRUPT  0x8E    // 64-bit interrupt gate, present, ring 0
#define IDT_TYPE_TRAP       0x8F    // 64-bit trap gate, present, ring 0
#define IDT_TYPE_USER       0xEE    // 64-bit interrupt gate, present, ring 3

// CPU Exception numbers
#define EXC_DIVIDE_BY_ZERO      0
#define EXC_DEBUG               1
#define EXC_NMI                 2
#define EXC_BREAKPOINT          3
#define EXC_OVERFLOW            4
#define EXC_BOUND_RANGE         5
#define EXC_INVALID_OPCODE      6
#define EXC_DEVICE_NOT_AVAILABLE 7
#define EXC_DOUBLE_FAULT        8
#define EXC_INVALID_TSS         10
#define EXC_SEGMENT_NOT_PRESENT 11
#define EXC_STACK_SEGMENT_FAULT 12
#define EXC_GENERAL_PROTECTION  13
#define EXC_PAGE_FAULT          14
#define EXC_X87_FPU_ERROR       16
#define EXC_ALIGNMENT_CHECK     17
#define EXC_MACHINE_CHECK       18
#define EXC_SIMD_FP_EXCEPTION   19
#define EXC_VIRTUALIZATION      20
#define EXC_SECURITY_EXCEPTION  30

// Interrupt stack frame (pushed by CPU on interrupt)
struct interrupt_frame {
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed));

// Functions
extern "C" void idt_init();
extern "C" void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector, uint8_t type_attr);

// Exception handlers (defined in interrupts.asm)
extern "C" void isr0();   // Divide by zero
extern "C" void isr1();   // Debug
extern "C" void isr2();   // NMI
extern "C" void isr3();   // Breakpoint
extern "C" void isr4();   // Overflow
extern "C" void isr5();   // Bound Range Exceeded
extern "C" void isr6();   // Invalid Opcode
extern "C" void isr7();   // Device Not Available
extern "C" void isr8();   // Double Fault
extern "C" void isr10();  // Invalid TSS
extern "C" void isr11();  // Segment Not Present
extern "C" void isr12();  // Stack Segment Fault
extern "C" void isr13();  // General Protection Fault
extern "C" void isr14();  // Page Fault
extern "C" void isr16();  // x87 FPU Error
extern "C" void isr17();  // Alignment Check
extern "C" void isr18();  // Machine Check
extern "C" void isr19();  // SIMD FP Exception
extern "C" void isr20();  // Virtualization Exception
extern "C" void isr30();  // Security Exception

// IRQ handlers (defined in interrupts.asm)
extern "C" void irq0();   // Timer
extern "C" void irq1();   // Keyboard
extern "C" void irq2();   // Cascade
extern "C" void irq3();   // COM2
extern "C" void irq4();   // COM1
extern "C" void irq5();   // LPT2
extern "C" void irq6();   // Floppy
extern "C" void irq7();   // LPT1
extern "C" void irq8();   // RTC
extern "C" void irq9();   // Free
extern "C" void irq10();  // Free
extern "C" void irq11();  // Free
extern "C" void irq12();  // PS/2 Mouse
extern "C" void irq13();  // FPU
extern "C" void irq14();  // ATA Primary
extern "C" void irq15();  // ATA Secondary

// Common exception handler (called from assembly)
extern "C" void exception_handler(uint64_t vector, uint64_t error_code, interrupt_frame* frame);

// Common IRQ handler (called from assembly)
extern "C" void irq_handler(uint64_t irq_num);

#endif
