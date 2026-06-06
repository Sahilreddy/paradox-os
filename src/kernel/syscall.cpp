#include "../include/syscall.h"
#include "../include/serial.h"
#include "../include/vga.h"
#include "../include/process.h"
#include "../include/timer.h"
#include "../include/usermode.h"
#include "../include/stdin.h"

extern "C" void usermode_return();

struct cpu_context;

typedef int64_t (*syscall_fn)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

#define MAX_SYSCALLS 16
static syscall_fn syscall_table[MAX_SYSCALLS];

static int64_t syscall_dispatch(uint64_t syscall_num, uint64_t rdi, uint64_t rsi,
                                uint64_t rdx, uint64_t r10, uint64_t r8) {
    if (syscall_num >= MAX_SYSCALLS || !syscall_table[syscall_num]) {
        serial_print("Syscall: invalid ");
        serial_print_hex(syscall_num);
        serial_print("\n");
        return -1;
    }
    return syscall_table[syscall_num](rdi, rsi, rdx, r10, r8);
}

// Only fd=0 wired up — blocking line read off the keyboard.
int64_t sys_read(int fd, void* buf, uint64_t count) {
    if (fd == 0) return (int64_t)stdin_read_line((char*)buf, count);
    return -1;
}

// `count` is the exact byte count; user-provided strings may contain NUL.
int64_t sys_write(int fd, const void* buf, uint64_t count) {
    if (fd == 1 || fd == 2) {
        const char* str = (const char*)buf;
        for (uint64_t i = 0; i < count; i++) vga_putchar(str[i]);
        return (int64_t)count;
    }
    return -1;
}

int64_t sys_exit(int status) {
    serial_print("Syscall: exit(");
    serial_print_hex(status);
    serial_print(")\n");

    if (usermode_active()) {
        usermode_return();   // does not return
    }

    process* current = process_get_current();
    if (current) {
        current->state = PROCESS_TERMINATED;
        scheduler_remove(current);
        scheduler_schedule();
        while (1) asm volatile("hlt");
    }
    return 0;
}

int64_t sys_hello() {
    vga_print("Hello from ring 3!\n");
    vga_print("This message came out of an int 0x80 syscall handler\n");
    vga_print("running in CPL=0, called by user code in CPL=3.\n");
    return 0;
}

int64_t sys_getpid() {
    process* current = process_get_current();
    return current ? current->pid : -1;
}

int64_t sys_sleep(uint32_t milliseconds) {
    timer_sleep(milliseconds);
    return 0;
}

int64_t sys_yield() {
    process_yield();
    return 0;
}

static int64_t syscall_read_wrapper(uint64_t fd, uint64_t buf, uint64_t count,
                                    uint64_t, uint64_t) {
    return sys_read((int)fd, (void*)buf, count);
}

static int64_t syscall_write_wrapper(uint64_t fd, uint64_t buf, uint64_t count,
                                     uint64_t, uint64_t) {
    return sys_write((int)fd, (const void*)buf, count);
}

static int64_t syscall_exit_wrapper(uint64_t status, uint64_t, uint64_t,
                                    uint64_t, uint64_t) {
    return sys_exit((int)status);
}

static int64_t syscall_getpid_wrapper(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return sys_getpid();
}

static int64_t syscall_sleep_wrapper(uint64_t ms, uint64_t, uint64_t, uint64_t, uint64_t) {
    return sys_sleep((uint32_t)ms);
}

static int64_t syscall_yield_wrapper(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return sys_yield();
}

static int64_t syscall_hello_wrapper(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) {
    return sys_hello();
}

void syscall_init() {
    serial_print("Syscall: init\n");

    for (int i = 0; i < MAX_SYSCALLS; i++) syscall_table[i] = nullptr;

    syscall_table[SYS_READ]   = syscall_read_wrapper;
    syscall_table[SYS_WRITE]  = syscall_write_wrapper;
    syscall_table[SYS_EXIT]   = syscall_exit_wrapper;
    syscall_table[SYS_GETPID] = syscall_getpid_wrapper;
    syscall_table[SYS_SLEEP]  = syscall_sleep_wrapper;
    syscall_table[SYS_YIELD]  = syscall_yield_wrapper;
    syscall_table[SYS_HELLO]  = syscall_hello_wrapper;

    serial_print("Syscall: ready\n");
}

extern "C" void syscall_handler_asm();

extern "C" int64_t syscall_handler_c(uint64_t syscall_num, uint64_t rdi, uint64_t rsi,
                                     uint64_t rdx, uint64_t r10, uint64_t r8) {
    return syscall_dispatch(syscall_num, rdi, rsi, rdx, r10, r8);
}

int64_t syscall_invoke(uint64_t syscall_num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    return syscall_dispatch(syscall_num, arg1, arg2, arg3, 0, 0);
}
