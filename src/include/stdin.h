// Line-buffered stdin for ring-3 SYS_READ. One line in flight at a time.

#ifndef STDIN_H
#define STDIN_H

#include "types.h"

void     stdin_init();
void     stdin_push(char c);
uint64_t stdin_read_line(char* out, uint64_t cap);

#endif
