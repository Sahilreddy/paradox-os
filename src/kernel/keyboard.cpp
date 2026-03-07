#include "../include/keyboard.h"
#include "../include/vga.h"
#include "../include/serial.h"
#include "../include/pic.h"
#include "../include/shell.h"

// I/O port operations
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Keyboard state
static uint8_t modifiers = 0;
static char key_buffer[256];
static uint8_t buffer_start = 0;
static uint8_t buffer_end = 0;

// US QWERTY scancode to ASCII map (scan code set 1)
static const char scancode_to_ascii[] = {
    0,   0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

// Shifted characters
static const char scancode_to_ascii_shift[] = {
    0,   0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' '
};

// Initialize keyboard
void keyboard_init() {
    // Clear keyboard buffer
    buffer_start = 0;
    buffer_end = 0;
    modifiers = 0;
    
    // Enable keyboard (unmask IRQ1)
    pic_clear_mask(1);
    
    serial_print("Keyboard: Initialized (PS/2)\n");
}

// Add character to buffer
static void buffer_add(char c) {
    uint8_t next = (buffer_end + 1) % 256;
    if (next != buffer_start) {
        key_buffer[buffer_end] = c;
        buffer_end = next;
    }
}

// Check if buffer has data
bool keyboard_has_key() {
    return buffer_start != buffer_end;
}

// Get character from buffer (blocking)
char keyboard_getchar() {
    while (!keyboard_has_key()) {
        asm volatile("hlt");
    }
    
    char c = key_buffer[buffer_start];
    buffer_start = (buffer_start + 1) % 256;
    return c;
}

// Keyboard interrupt handler (called from IRQ1 stub)
void keyboard_handler() {
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    
    // Check if this is a key release (bit 7 set)
    if (scancode & 0x80) {
        // Key released
        scancode &= 0x7F;  // Clear release bit
        
        // Update modifier flags
        if (scancode == KEY_LSHIFT || scancode == KEY_RSHIFT) {
            modifiers &= ~MOD_SHIFT;
        } else if (scancode == KEY_LCTRL) {
            modifiers &= ~MOD_CTRL;
        } else if (scancode == KEY_LALT) {
            modifiers &= ~MOD_ALT;
        }
    } else {
        // Key pressed
        
        // Update modifier flags
        if (scancode == KEY_LSHIFT || scancode == KEY_RSHIFT) {
            modifiers |= MOD_SHIFT;
        } else if (scancode == KEY_LCTRL) {
            modifiers |= MOD_CTRL;
        } else if (scancode == KEY_LALT) {
            modifiers |= MOD_ALT;
        } else if (scancode == KEY_CAPSLOCK) {
            modifiers ^= MOD_CAPSLOCK;  // Toggle
        } else {
            // Convert scancode to ASCII
            char c = 0;
            
            if (scancode < sizeof(scancode_to_ascii)) {
                if (modifiers & MOD_SHIFT) {
                    c = scancode_to_ascii_shift[scancode];
                } else {
                    c = scancode_to_ascii[scancode];
                    
                    // Apply CAPS LOCK to letters
                    if ((modifiers & MOD_CAPSLOCK) && c >= 'a' && c <= 'z') {
                        c = c - 'a' + 'A';
                    }
                }
                
                // Handle special key combinations
                if (modifiers & MOD_CTRL) {
                    if (c >= 'a' && c <= 'z') {
                        c = c - 'a' + 1;  // Ctrl+A = 1, Ctrl+B = 2, etc.
                    } else if (c >= 'A' && c <= 'Z') {
                        c = c - 'A' + 1;
                    }
                }
                
                // Add to buffer and send to shell
                if (c != 0) {
                    buffer_add(c);
                    shell_process_char(c);
                    serial_putchar(c); // Also log to serial
                }
            }
        }
    }
    
    // Send End Of Interrupt to PIC
    pic_send_eoi(1);
}
