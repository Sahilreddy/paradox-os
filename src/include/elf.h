// Static ELF64 loader. No relocations, no dynamic linking.

#ifndef ELF_H
#define ELF_H

#include "types.h"

// Returns entry RIP on success, 0 on failure. Logs to serial either way.
uint64_t elf_load(const uint8_t* image, uint32_t len);

#endif
