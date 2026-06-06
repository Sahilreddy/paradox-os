// Drop into ring 3 and back. Returns when the user program calls SYS_EXIT.

#ifndef USERMODE_H
#define USERMODE_H

#include "types.h"

bool        usermode_active();
const char* usermode_status();

void usermode_run(uint64_t entry_rip, uint64_t user_rsp);

// Allocates a one-page user stack, builds a SysV argc/argv frame
// (argv[0] = prog_name, then whitespace-split tokens from args).
void usermode_run_with_args(uint64_t entry_rip,
                            const char* prog_name,
                            const char* args);

extern "C" void usermode_return();

#endif
