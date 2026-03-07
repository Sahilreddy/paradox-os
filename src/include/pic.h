#ifndef PIC_H
#define PIC_H

#include "types.h"

// PIC (Programmable Interrupt Controller) 8259 chip
// We have 2 PICs: master and slave (cascaded)

// Master PIC ports
#define PIC1_COMMAND    0x20
#define PIC1_DATA       0x21

// Slave PIC ports
#define PIC2_COMMAND    0xA0
#define PIC2_DATA       0xA1

// PIC commands
#define PIC_EOI         0x20    // End Of Interrupt

// Initialization Command Words
#define ICW1_ICW4       0x01    // ICW4 needed
#define ICW1_SINGLE     0x02    // Single (cascade) mode
#define ICW1_INTERVAL4  0x04    // Call address interval 4 (8)
#define ICW1_LEVEL      0x08    // Level triggered (edge) mode
#define ICW1_INIT       0x10    // Initialization required

#define ICW4_8086       0x01    // 8086/88 (MCS-80/85) mode
#define ICW4_AUTO       0x02    // Auto (normal) EOI
#define ICW4_BUF_SLAVE  0x08    // Buffered mode/slave
#define ICW4_BUF_MASTER 0x0C    // Buffered mode/master
#define ICW4_SFNM       0x10    // Special fully nested (not)

// IRQ numbers
#define IRQ0            32      // Timer
#define IRQ1            33      // Keyboard
#define IRQ2            34      // Cascade (never raised)
#define IRQ3            35      // COM2
#define IRQ4            36      // COM1
#define IRQ5            37      // LPT2
#define IRQ6            38      // Floppy disk
#define IRQ7            39      // LPT1 / spurious
#define IRQ8            40      // CMOS real-time clock
#define IRQ9            41      // Free
#define IRQ10           42      // Free
#define IRQ11           43      // Free
#define IRQ12           44      // PS/2 Mouse
#define IRQ13           45      // FPU / Coprocessor
#define IRQ14           46      // Primary ATA
#define IRQ15           47      // Secondary ATA

// Functions
void pic_init();
void pic_send_eoi(uint8_t irq);
void pic_disable_all();
void pic_set_mask(uint8_t irq);
void pic_clear_mask(uint8_t irq);

#endif
