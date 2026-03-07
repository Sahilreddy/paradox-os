// ParadoxOS VGA Text Mode Driver

#include "../include/vga.h"

static uint16_t* vga_buffer = (uint16_t*)0xB8000;
static uint32_t vga_row = 0;
static uint32_t vga_col = 0;
static uint8_t vga_color = VGA_COLOR_WHITE | (VGA_COLOR_BLACK << 4);

void vga_init() {
    vga_row = 0;
    vga_col = 0;
    vga_color = VGA_COLOR_WHITE | (VGA_COLOR_BLACK << 4);
    vga_clear();
}

void vga_clear() {
    for (uint32_t y = 0; y < VGA_HEIGHT; y++) {
        for (uint32_t x = 0; x < VGA_WIDTH; x++) {
            const uint32_t index = y * VGA_WIDTH + x;
            vga_buffer[index] = ' ' | (vga_color << 8);
        }
    }
    vga_row = 0;
    vga_col = 0;
}

void vga_setcolor(uint8_t fg, uint8_t bg) {
    vga_color = fg | (bg << 4);
}

void vga_putchar(char c) {
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
    } else if (c == '\t') {
        vga_col = (vga_col + 8) & ~7;
    } else if (c == '\r') {
        vga_col = 0;
    } else if (c == '\b') {
        if (vga_col > 0) {
            vga_col--;
        }
    } else {
        const uint32_t index = vga_row * VGA_WIDTH + vga_col;
        vga_buffer[index] = c | (vga_color << 8);
        vga_col++;
    }
    
    if (vga_col >= VGA_WIDTH) {
        vga_col = 0;
        vga_row++;
    }
    
    if (vga_row >= VGA_HEIGHT) {
        vga_scroll();
    }
}

void vga_print(const char* str) {
    for (uint32_t i = 0; str[i] != '\0'; i++) {
        vga_putchar(str[i]);
    }
}

void vga_scroll() {
    // Move all lines up by one
    for (uint32_t y = 0; y < VGA_HEIGHT - 1; y++) {
        for (uint32_t x = 0; x < VGA_WIDTH; x++) {
            const uint32_t dst = y * VGA_WIDTH + x;
            const uint32_t src = (y + 1) * VGA_WIDTH + x;
            vga_buffer[dst] = vga_buffer[src];
        }
    }
    
    // Clear last line
    for (uint32_t x = 0; x < VGA_WIDTH; x++) {
        const uint32_t index = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
        vga_buffer[index] = ' ' | (vga_color << 8);
    }
    
    vga_row = VGA_HEIGHT - 1;
}

void vga_print_hex(uint32_t value) {
    const char hex_chars[] = "0123456789ABCDEF";
    char buffer[9];
    buffer[8] = '\0';
    
    for (int i = 7; i >= 0; i--) {
        buffer[i] = hex_chars[value & 0xF];
        value >>= 4;
    }
    
    vga_print(buffer);
}

void vga_print_dec(uint64_t value) {
    if (value == 0) {
        vga_putchar('0');
        return;
    }
    
    char buffer[21]; // Enough for 64-bit number
    int i = 20;
    buffer[i] = '\0';
    
    while (value > 0 && i > 0) {
        i--;
        buffer[i] = '0' + (value % 10);
        value /= 10;
    }
    
    vga_print(&buffer[i]);
}

// Print a byte in hex (2 digits)
void vga_print_hex_byte(uint8_t num) {
    const char* hex = "0123456789ABCDEF";
    vga_putchar(hex[(num >> 4) & 0xF]);
    vga_putchar(hex[num & 0xF]);
}

// Print a word in hex (4 digits)
void vga_print_hex_word(uint16_t num) {
    const char* hex = "0123456789ABCDEF";
    vga_putchar(hex[(num >> 12) & 0xF]);
    vga_putchar(hex[(num >> 8) & 0xF]);
    vga_putchar(hex[(num >> 4) & 0xF]);
    vga_putchar(hex[num & 0xF]);
}
