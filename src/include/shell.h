#ifndef SHELL_H
#define SHELL_H

#include "types.h"

// Shell configuration
#define SHELL_MAX_COMMAND_LENGTH 256
#define SHELL_PROMPT "paradox> "

// Initialize the shell
void shell_init();

// Process a character from keyboard
void shell_process_char(char c);

// Execute a command
void shell_execute(const char* command);

#endif // SHELL_H
