#ifndef SYSCALL_H
#define SYSCALL_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declaration
struct cpu_context;

// System call numbers
#define SYS_READ    0
#define SYS_WRITE   1
#define SYS_OPEN    2
#define SYS_CLOSE   3
#define SYS_EXIT    4
#define SYS_FORK    5
#define SYS_EXEC    6
#define SYS_GETPID  7
#define SYS_SLEEP   8
#define SYS_YIELD   9

// Initialize system calls
void syscall_init();

// System call implementations
int64_t sys_read(int fd, void* buf, uint64_t count);
int64_t sys_write(int fd, const void* buf, uint64_t count);
int64_t sys_exit(int status);
int64_t sys_getpid();
int64_t sys_sleep(uint32_t milliseconds);
int64_t sys_yield();

// Invoke a system call from kernel mode (for testing)
int64_t syscall_invoke(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3);

#ifdef __cplusplus
}
#endif

#endif // SYSCALL_H
