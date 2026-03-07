// ParadoxOS Kernel Header

#ifndef KERNEL_H
#define KERNEL_H

#include "types.h"

// Kernel main function
extern "C" void kernel_main(unsigned long magic, unsigned long addr);

// Utility functions
void kernel_panic(const char* message);

#endif // KERNEL_H
