#ifndef PROCESS_H
#define PROCESS_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Process states
enum process_state {
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_TERMINATED
};

// CPU context for context switching
struct cpu_context {
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11;
    uint64_t r12, r13, r14, r15;
    uint64_t rip;
    uint64_t rflags;
    uint64_t cr3;  // Page table
} __attribute__((packed));

// Process Control Block
struct process {
    uint32_t pid;                    // Process ID
    char name[32];                   // Process name
    process_state state;             // Current state
    cpu_context context;             // Saved CPU state
    uint64_t kernel_stack;           // Kernel stack pointer
    uint64_t user_stack;             // User stack pointer
    uint32_t priority;               // Process priority
    uint64_t time_slice;             // Remaining time slice
    process* next;                   // Next process in list
};

// Process management functions
void process_init();
process* process_create(const char* name, void (*entry_point)());
void process_destroy(process* proc);
void process_yield();
process* process_get_current();
process* process_get_by_pid(uint32_t pid);
process* process_create_test();

// Scheduler
void scheduler_init();
void scheduler_add(process* proc);
void scheduler_remove(process* proc);
void scheduler_schedule();
process* scheduler_next();

// Context switching
extern "C" void switch_context(cpu_context* old_ctx, cpu_context* new_ctx);

#ifdef __cplusplus
}
#endif

#endif // PROCESS_H
