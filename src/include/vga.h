// ParadoxOS VGA Driver Header

#ifndef VGA_H
#define VGA_H

#include "types.h"

// VGA text mode constants
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

// VGA color codes
enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,  // Also known as yellow
    VGA_COLOR_YELLOW = 14,        // Alias for light brown
    VGA_COLOR_LIGHT_YELLOW = 14,  // Alias for light brown
    VGA_COLOR_WHITE = 15,
};

// VGA functions
void vga_init();
void vga_clear();
void vga_setcolor(uint8_t fg, uint8_t bg);
void vga_putchar(char c);
void vga_print(const char* str);
void vga_scroll();
void vga_print_hex(uint32_t value);
void vga_print_dec(uint64_t value);
void vga_print_hex_byte(uint8_t num);
void vga_print_hex_word(uint16_t num);

#endif // VGA_H
