// PIT Timer Driver
#include "../include/timer.h"
#include "../include/serial.h"
#include "../include/idt.h"
#include "../include/pic.h"
#include "../include/process.h"

// I/O port operations
static inline void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static volatile uint64_t timer_ticks = 0;
static uint32_t timer_frequency = 0;

// Initialize PIT timer
void timer_init(uint32_t frequency) {
    serial_print("Timer: Initializing PIT at ");
    serial_print_hex(frequency);
    serial_print(" Hz\n");
    
    timer_frequency = frequency;
    
    // Calculate divisor for desired frequency
    uint32_t divisor = PIT_FREQUENCY / frequency;
    
    if (divisor > 65535) {
        divisor = 65535;  // Maximum divisor
    }
    
    // Send command byte:
    // - Channel 0
    // - Access mode: lobyte/hibyte
    // - Operating mode: rate generator
    // - Binary mode
    uint8_t command = PIT_CHANNEL0_SEL | PIT_LOHI | PIT_MODE2 | PIT_BINARY;
    outb(PIT_COMMAND, command);
    
    // Send divisor (low byte, then high byte)
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
    
    serial_print("Timer: PIT configured with divisor ");
    serial_print_hex(divisor);
    serial_print(" (actual frequency: ");
    serial_print_hex(PIT_FREQUENCY / divisor);
    serial_print(" Hz)\n");
    
    // Enable timer IRQ (IRQ0)
    pic_clear_mask(0);
    
    serial_print("Timer: Initialized successfully!\n");
}

// Timer interrupt handler
extern "C" void timer_handler() {
    timer_ticks++;
    
    // Every 10 ticks (100ms at 100Hz), schedule next process
    if (timer_ticks % 10 == 0) {
        scheduler_schedule();
    }
}

// Get current tick count
uint64_t timer_get_ticks() {
    return timer_ticks;
}

// Sleep for specified milliseconds (busy wait for now)
void timer_sleep(uint32_t milliseconds) {
    uint64_t target = timer_ticks + (milliseconds * timer_frequency / 1000);
    while (timer_ticks < target) {
        asm volatile("hlt");  // Halt until next interrupt
    }
}
