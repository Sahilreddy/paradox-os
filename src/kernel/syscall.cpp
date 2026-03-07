// System Call Handler
#include "../include/syscall.h"
#include "../include/serial.h"
#include "../include/vga.h"
#include "../include/process.h"
#include "../include/timer.h"

// Forward declarations
struct cpu_context;

// System call function pointer type
typedef int64_t (*syscall_fn)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

// System call table
#define MAX_SYSCALLS 16
static syscall_fn syscall_table[MAX_SYSCALLS];

// Helper function to convert context registers to syscall args
static int64_t syscall_dispatch(uint64_t syscall_num, uint64_t rdi, uint64_t rsi, 
                                uint64_t rdx, uint64_t r10, uint64_t r8) {
    if (syscall_num >= MAX_SYSCALLS || !syscall_table[syscall_num]) {
        serial_print("Syscall: Invalid syscall number ");
        serial_print_hex(syscall_num);
        serial_print("\n");
        return -1;
    }
    
    return syscall_table[syscall_num](rdi, rsi, rdx, r10, r8);
}

// SYS_READ - Read from file descriptor
int64_t sys_read(int fd, void* buf, uint64_t count) {
    serial_print("Syscall: read(fd=");
    serial_print_hex(fd);
    serial_print(", buf=");
    serial_print_hex((uint64_t)buf);
    serial_print(", count=");
    serial_print_hex(count);
    serial_print(")\n");
    
    // TODO: Implement actual file I/O
    return -1;  // Not implemented yet
}

// SYS_WRITE - Write to file descriptor
int64_t sys_write(int fd, const void* buf, uint64_t count) {
    // For now, only support stdout (fd=1) and stderr (fd=2)
    if (fd == 1 || fd == 2) {
        const char* str = (const char*)buf;
        for (uint64_t i = 0; i < count; i++) {
            if (str[i] == '\0') break;
            // Use serial print for now
            char temp[2] = {str[i], '\0'};
            serial_print(temp);
            vga_putchar(str[i]);
        }
        return count;
    }
    
    serial_print("Syscall: write to unsupported fd ");
    serial_print_hex(fd);
    serial_print("\n");
    return -1;
}

// SYS_EXIT - Terminate current process
int64_t sys_exit(int status) {
    serial_print("Syscall: exit(");
    serial_print_hex(status);
    serial_print(") from PID ");
    
    process* current = process_get_current();
    if (current) {
        serial_print_hex(current->pid);
        serial_print("\n");
        
        // Mark process as terminated
        current->state = PROCESS_TERMINATED;
        
        // Remove from scheduler
        scheduler_remove(current);
        
        serial_print("Process ");
        serial_print_hex(current->pid);
        serial_print(" terminated, switching to next process\n");
        
        // Switch to next process (will not return here)
        scheduler_schedule();
        
        // Should never reach here
        while (1) {
            asm volatile("hlt");
        }
    }
    
    serial_print("(no current process)\n");
    return 0;
}

// SYS_GETPID - Get current process ID
int64_t sys_getpid() {
    process* current = process_get_current();
    if (current) {
        return current->pid;
    }
    return -1;
}

// SYS_SLEEP - Sleep for milliseconds
int64_t sys_sleep(uint32_t milliseconds) {
    timer_sleep(milliseconds);
    return 0;
}

// SYS_YIELD - Yield CPU to another process
int64_t sys_yield() {
    process_yield();
    return 0;
}

// Wrapper functions for syscall table
static int64_t syscall_read_wrapper(uint64_t fd, uint64_t buf, uint64_t count, 
                                    uint64_t unused1, uint64_t unused2) {
    (void)unused1; (void)unused2;
    return sys_read((int)fd, (void*)buf, count);
}

static int64_t syscall_write_wrapper(uint64_t fd, uint64_t buf, uint64_t count,
                                     uint64_t unused1, uint64_t unused2) {
    (void)unused1; (void)unused2;
    return sys_write((int)fd, (const void*)buf, count);
}

static int64_t syscall_exit_wrapper(uint64_t status, uint64_t unused1, uint64_t unused2,
                                    uint64_t unused3, uint64_t unused4) {
    (void)unused1; (void)unused2; (void)unused3; (void)unused4;
    return sys_exit((int)status);
}

static int64_t syscall_getpid_wrapper(uint64_t unused1, uint64_t unused2, uint64_t unused3,
                                      uint64_t unused4, uint64_t unused5) {
    (void)unused1; (void)unused2; (void)unused3; (void)unused4; (void)unused5;
    return sys_getpid();
}

static int64_t syscall_sleep_wrapper(uint64_t ms, uint64_t unused1, uint64_t unused2,
                                     uint64_t unused3, uint64_t unused4) {
    (void)unused1; (void)unused2; (void)unused3; (void)unused4;
    return sys_sleep((uint32_t)ms);
}

static int64_t syscall_yield_wrapper(uint64_t unused1, uint64_t unused2, uint64_t unused3,
                                     uint64_t unused4, uint64_t unused5) {
    (void)unused1; (void)unused2; (void)unused3; (void)unused4; (void)unused5;
    return sys_yield();
}

// Initialize system call table
void syscall_init() {
    serial_print("Syscall: Initializing system call interface...\n");
    
    // Clear syscall table
    for (int i = 0; i < MAX_SYSCALLS; i++) {
        syscall_table[i] = nullptr;
    }
    
    // Register system calls
    syscall_table[SYS_READ]    = syscall_read_wrapper;
    syscall_table[SYS_WRITE]   = syscall_write_wrapper;
    syscall_table[SYS_EXIT]    = syscall_exit_wrapper;
    syscall_table[SYS_GETPID]  = syscall_getpid_wrapper;
    syscall_table[SYS_SLEEP]   = syscall_sleep_wrapper;
    syscall_table[SYS_YIELD]   = syscall_yield_wrapper;
    
    serial_print("Syscall: Registered ");
    serial_print_hex(6);
    serial_print(" system calls\n");
    serial_print("Syscall: System call interface initialized!\n");
}

// System call handler - called from INT 0x80
extern "C" void syscall_handler_asm();

// C++ system call handler
extern "C" int64_t syscall_handler_c(uint64_t syscall_num, uint64_t rdi, uint64_t rsi,
                                     uint64_t rdx, uint64_t r10, uint64_t r8) {
    return syscall_dispatch(syscall_num, rdi, rsi, rdx, r10, r8);
}

// Invoke system call from kernel mode (for testing)
int64_t syscall_invoke(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    return syscall_dispatch(syscall_num, arg1, arg2, arg3, 0, 0);
}
