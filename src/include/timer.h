#ifndef TIMER_H
#define TIMER_H

#include "types.h"

// PIT (Programmable Interval Timer) I/O ports
#define PIT_CHANNEL0    0x40
#define PIT_CHANNEL1    0x41
#define PIT_CHANNEL2    0x42
#define PIT_COMMAND     0x43

// PIT frequency (1.193182 MHz)
#define PIT_FREQUENCY   1193182

// PIT command byte bits
#define PIT_BINARY      0x00    // Binary mode
#define PIT_BCD         0x01    // BCD mode
#define PIT_MODE0       0x00    // Interrupt on terminal count
#define PIT_MODE2       0x04    // Rate generator (for timer)
#define PIT_MODE3       0x06    // Square wave generator
#define PIT_LOBYTE      0x10    // Access low byte only
#define PIT_HIBYTE      0x20    // Access high byte only
#define PIT_LOHI        0x30    // Access low then high byte
#define PIT_CHANNEL0_SEL 0x00   // Select channel 0

#ifdef __cplusplus
extern "C" {
#endif

// Timer functions
void timer_init(uint32_t frequency);
void timer_handler();
uint64_t timer_get_ticks();
void timer_sleep(uint32_t milliseconds);

#ifdef __cplusplus
}
#endif

#endif // TIMER_H
