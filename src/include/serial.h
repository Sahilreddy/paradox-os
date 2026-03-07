// ParadoxOS Serial Port Driver Header

#ifndef SERIAL_H
#define SERIAL_H

#include "types.h"

// COM1 port
#define COM1 0x3F8

// Serial functions
void serial_init();
void serial_putchar(char c);
void serial_print(const char* str);
void serial_print_hex(uint32_t value);

#endif // SERIAL_H
