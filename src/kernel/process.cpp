// Process Management System
#include "../include/process.h"
#include "../include/memory.h"
#include "../include/serial.h"
#include "../include/vga.h"

// Process table
#define MAX_PROCESSES 64
static process* process_table[MAX_PROCESSES];
static uint32_t next_pid = 1;
static process* current_process = nullptr;

// Scheduler queue
static process* ready_queue_head = nullptr;
static process* ready_queue_tail = nullptr;

// Initialize process management
void process_init() {
    serial_print("Process: Initializing process management...\n");
    
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_table[i] = nullptr;
    }
    
    // Create idle process (PID 0)
    current_process = (process*)kmalloc(sizeof(process));
    if (!current_process) {
        serial_print("Process: ERROR - Failed to allocate idle process!\n");
        return;
    }
    
    current_process->pid = 0;
    for (int i = 0; i < 32; i++) current_process->name[i] = 0;
    current_process->name[0] = 'i'; current_process->name[1] = 'd'; 
    current_process->name[2] = 'l'; current_process->name[3] = 'e';
    current_process->state = PROCESS_RUNNING;
    current_process->priority = 0;
    current_process->time_slice = 0;
    current_process->next = nullptr;
    
    process_table[0] = current_process;
    
    serial_print("Process: Idle process created (PID 0)\n");
}

// Create a new process
process* process_create(const char* name, void (*entry_point)()) {
    // Find free slot
    uint32_t slot = 0;
    for (slot = 0; slot < MAX_PROCESSES; slot++) {
        if (process_table[slot] == nullptr) break;
    }
    
    if (slot >= MAX_PROCESSES) {
        serial_print("Process: ERROR - Process table full!\n");
        return nullptr;
    }
    
    // Allocate PCB
    process* proc = (process*)kmalloc(sizeof(process));
    if (!proc) {
        serial_print("Process: ERROR - Failed to allocate PCB!\n");
        return nullptr;
    }
    
    // Initialize PCB
    proc->pid = next_pid++;
    
    // Copy name
    int i;
    for (i = 0; i < 31 && name[i]; i++) {
        proc->name[i] = name[i];
    }
    proc->name[i] = '\0';
    
    proc->state = PROCESS_READY;
    proc->priority = 1;
    proc->time_slice = 10;  // 10 timer ticks
    proc->next = nullptr;
    
    // Allocate kernel stack (1 page)
    proc->kernel_stack = (uint64_t)pmm_alloc_page();
    if (!proc->kernel_stack) {
        serial_print("Process: ERROR - Failed to allocate kernel stack!\n");
        kfree(proc);
        return nullptr;
    }
    proc->kernel_stack += PAGE_SIZE;  // Stack grows down
    
    // Allocate user stack (1 page)
    proc->user_stack = (uint64_t)pmm_alloc_page();
    if (!proc->user_stack) {
        serial_print("Process: ERROR - Failed to allocate user stack!\n");
        pmm_free_page((void*)(proc->kernel_stack - PAGE_SIZE));
        kfree(proc);
        return nullptr;
    }
    proc->user_stack += PAGE_SIZE;  // Stack grows down
    
    // Initialize context
    for (int j = 0; j < 16; j++) {
        ((uint64_t*)&proc->context)[j] = 0;
    }
    
    proc->context.rip = (uint64_t)entry_point;
    proc->context.rsp = proc->user_stack;
    proc->context.rflags = 0x202;  // Interrupts enabled
    
    // Get current CR3
    uint64_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    proc->context.cr3 = cr3;
    
    // Add to process table
    process_table[slot] = proc;
    
    serial_print("Process: Created process '");
    serial_print(proc->name);
    serial_print("' (PID ");
    serial_print_hex(proc->pid);
    serial_print(")\n");
    
    return proc;
}

// Destroy a process
void process_destroy(process* proc) {
    if (!proc) return;
    
    serial_print("Process: Destroying process ");
    serial_print_hex(proc->pid);
    serial_print("\n");
    
    // Remove from process table
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i] == proc) {
            process_table[i] = nullptr;
            break;
        }
    }
    
    // Free stacks
    if (proc->kernel_stack) {
        pmm_free_page((void*)(proc->kernel_stack - PAGE_SIZE));
    }
    if (proc->user_stack) {
        pmm_free_page((void*)(proc->user_stack - PAGE_SIZE));
    }
    
    // Free PCB
    kfree(proc);
}

// Get current process
process* process_get_current() {
    return current_process;
}

// Get process by PID
process* process_get_by_pid(uint32_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i] && process_table[i]->pid == pid) {
            return process_table[i];
        }
    }
    return nullptr;
}

// Initialize scheduler
void scheduler_init() {
    serial_print("Scheduler: Initialized round-robin scheduler\n");
}

// Add process to ready queue
void scheduler_add(process* proc) {
    if (!proc) return;
    
    proc->state = PROCESS_READY;
    proc->next = nullptr;
    
    if (!ready_queue_head) {
        ready_queue_head = proc;
        ready_queue_tail = proc;
    } else {
        ready_queue_tail->next = proc;
        ready_queue_tail = proc;
    }
    
    serial_print("Scheduler: Added process ");
    serial_print_hex(proc->pid);
    serial_print(" to ready queue\n");
}

// Remove process from ready queue
void scheduler_remove(process* proc) {
    if (!proc || !ready_queue_head) return;
    
    if (ready_queue_head == proc) {
        ready_queue_head = proc->next;
        if (ready_queue_tail == proc) {
            ready_queue_tail = nullptr;
        }
        return;
    }
    
    process* current = ready_queue_head;
    while (current->next) {
        if (current->next == proc) {
            current->next = proc->next;
            if (ready_queue_tail == proc) {
                ready_queue_tail = current;
            }
            return;
        }
        current = current->next;
    }
}

// Get next process to run
process* scheduler_next() {
    if (!ready_queue_head) {
        return process_table[0];  // Return idle process
    }
    
    process* next = ready_queue_head;
    ready_queue_head = next->next;
    
    if (!ready_queue_head) {
        ready_queue_tail = nullptr;
    }
    
    return next;
}

// Schedule next process
void scheduler_schedule() {
    // Save current process
    process* old_process = current_process;
    
    if (old_process && old_process->state == PROCESS_RUNNING) {
        old_process->state = PROCESS_READY;
        scheduler_add(old_process);
    }
    
    // Get next process
    process* new_process = scheduler_next();
    if (!new_process) {
        new_process = process_table[0];  // Idle
    }
    
    if (new_process == old_process) {
        return;  // Same process, no switch needed
    }
    
    new_process->state = PROCESS_RUNNING;
    current_process = new_process;
    
    serial_print("Scheduler: Switching from PID ");
    serial_print_hex(old_process ? old_process->pid : 0);
    serial_print(" to PID ");
    serial_print_hex(new_process->pid);
    serial_print("\n");
    
    // Perform context switch
    if (old_process) {
        switch_context(&old_process->context, &new_process->context);
    }
}

// Yield CPU to another process
void process_yield() {
    scheduler_schedule();
}

// Test process function - prints a message periodically
static void test_process_func() {
    uint64_t counter = 0;
    while (counter < 5) {  // Run 5 iterations then exit
        serial_print("Test Process (PID ");
        serial_print_hex(process_get_current()->pid);
        serial_print("): Counter = ");
        serial_print_hex(counter++);
        serial_print("\n");
        
        // Busy wait a bit
        for (volatile uint64_t i = 0; i < 5000000; i++);
    }
    
    // Exit after 5 iterations
    serial_print("Test Process (PID ");
    serial_print_hex(process_get_current()->pid);
    serial_print("): Exiting...\n");
    
    // Call exit syscall
    asm volatile(
        "mov $4, %%rax\n"     // SYS_EXIT = 4
        "mov $0, %%rdi\n"     // exit status = 0
        "int $0x80\n"
        : : : "rax", "rdi"
    );
    
    // Should never reach here
    while (1) asm("hlt");
}

// Create a test process
process* process_create_test() {
    return process_create("test", test_process_func);
}

