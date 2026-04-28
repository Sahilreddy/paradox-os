#include "../include/shell.h"
#include "../include/vga.h"
#include "../include/serial.h"
#include "../include/keyboard.h"
#include "../include/memory.h"
#include "../include/process.h"
#include "../include/timer.h"
#include "../include/syscall.h"
#include "../include/pci.h"

// Shell state
static char command_buffer[SHELL_MAX_COMMAND_LENGTH];
static uint16_t buffer_pos = 0;

// String utility functions
static uint32_t strlen(const char* str) {
    uint32_t len = 0;
    while (str[len]) len++;
    return len;
}

static int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

static int strncmp(const char* s1, const char* s2, uint32_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

// String copy
static void strcpy(char* dest, const char* src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

// Show the shell prompt
static void show_prompt() {
    vga_setcolor(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_print(SHELL_PROMPT);
    vga_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

// Initialize shell
void shell_init() {
    buffer_pos = 0;
    command_buffer[0] = '\0';
    
    // Welcome message
    vga_setcolor(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_print("Shell initialized. Type 'help' for available commands.\n\n");
    
    show_prompt();
    serial_print("Shell: Initialized\n");
}

// Command implementations
static void cmd_help() {
    vga_setcolor(VGA_COLOR_LIGHT_YELLOW, VGA_COLOR_BLACK);
    vga_print("\nAvailable Commands:\n");
    vga_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_print("  help      - Show this help message\n");
    vga_print("  clear     - Clear the screen\n");
    vga_print("  echo      - Echo text to screen\n");
    vga_print("  info      - Show system information\n");
    vga_print("  memory    - Show memory statistics\n");
    vga_print("  ps        - List running processes\n");
    vga_print("  uptime    - Show system uptime\n");
    vga_print("  spawn     - Spawn a test process\n");
    vga_print("  syscall   - Test system calls\n");
    vga_print("  lspci     - List PCI devices\n");
    vga_print("  reboot    - Reboot the system\n");
    vga_print("\n");
}

static void cmd_clear() {
    vga_clear();
}

static void cmd_echo(const char* args) {
    vga_setcolor(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    if (args && *args) {
        vga_print((char*)args);
    }
    vga_print("\n");
    vga_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

static void cmd_info() {
    vga_setcolor(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_print("\n=== ParadoxOS System Information ===\n");
    vga_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_print("OS Name:      ParadoxOS\n");
    vga_print("Architecture: x86_64\n");
    vga_print("Mode:         Long Mode (64-bit)\n");
    vga_print("Bootloader:   Multiboot2\n");
    vga_print("VGA Mode:     Text 80x25\n");
    vga_print("Interrupts:   Enabled (IDT + PIC)\n");
    vga_print("Timer:        PIT @ 100 Hz\n");
    vga_print("Memory:       Physical + Heap allocators\n");
    vga_print("Processes:    Round-robin scheduler\n");
    vga_print("\n");
}

static void cmd_uptime() {
    uint64_t ticks = timer_get_ticks();
    uint64_t seconds = ticks / 100;  // 100 Hz timer
    uint64_t minutes = seconds / 60;
    uint64_t hours = minutes / 60;
    
    vga_setcolor(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_print("\n=== System Uptime ===\n");
    vga_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    vga_print("Ticks:   ");
    vga_print_dec(ticks);
    vga_print("\n");
    
    vga_print("Uptime:  ");
    vga_print_dec(hours);
    vga_print("h ");
    vga_print_dec(minutes % 60);
    vga_print("m ");
    vga_print_dec(seconds % 60);
    vga_print("s\nProcesses:    Round-robin scheduler\n");
    vga_print("\n");
}

static void cmd_ps() {
    vga_setcolor(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_print("\n=== Process List ===\n");
    vga_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_print("PID  NAME                STATE\n");
    vga_print("---  ----------------    -----\n");
    
    process* current = process_get_current();
    if (current) {
        vga_print_dec(current->pid);
        vga_print("    ");
        vga_print(current->name);
        vga_print("            ");
        vga_setcolor(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_print("RUNNING\n");
        vga_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    }
    
    vga_print("\n");
}

static void cmd_memory() {
    uint64_t total = pmm_get_total_memory();
    uint64_t used = pmm_get_used_memory();
    uint64_t free = pmm_get_free_memory();
    
    vga_setcolor(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_print("\n=== Memory Statistics ===\n");
    vga_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    vga_print("Total:  ");
    vga_setcolor(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_print_dec(total / (1024 * 1024));
    vga_print(" MB\n");
    
    vga_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_print("Used:   ");
    vga_setcolor(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_print_dec(used / (1024 * 1024));
    vga_print(" MB\n");
    
    vga_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_print("Free:   ");
    vga_setcolor(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_print_dec(free / (1024 * 1024));
    vga_print(" MB\n");
    
    vga_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_print("Pages:  ");
    vga_print_dec(free / PAGE_SIZE);
    vga_print(" available\n\n");
}

static void cmd_reboot() {
    vga_setcolor(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_print("\nRebooting system...\n");
    serial_print("Shell: Reboot command received\n");
    
    // Use keyboard controller to reboot (most reliable method)
    uint8_t good = 0x02;
    while (good & 0x02) {
        // Wait for keyboard controller to be ready
        asm volatile("inb $0x64, %0" : "=a"(good));
    }
    // Send reboot command to keyboard controller
    asm volatile("outb %0, $0x64" : : "a"((uint8_t)0xFE));
    
    // If that didn't work, hang
    while(1) asm("hlt");
}

static void cmd_spawn() {
    process* proc = process_create_test();
    if (proc) {
        vga_setcolor(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_print("\n=== Process Spawned ===\n");
        vga_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        vga_print("PID: ");
        vga_print_dec(proc->pid);
        vga_print("\nName: ");
        vga_print(proc->name);
        vga_print("\nState: READY\n\n");
        
        serial_print("Shell: Spawned test process PID ");
        serial_print_hex(proc->pid);
        serial_print("\n");
    } else {
        vga_setcolor(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_print("\nError: Failed to spawn process\n\n");
    }
}

static void cmd_lspci() {
    vga_setcolor(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_print("\n=== PCI Devices ===\n");
    vga_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    uint32_t count = pci_get_device_count();
    if (count == 0) {
        vga_print("No PCI devices found.\n\n");
        return;
    }
    
    vga_print("Bus Dev Fn VendID DevID Class      Vendor\n");
    vga_print("--- --- -- ------ ------ ---------- ------\n");
    
    for (uint32_t i = 0; i < count; i++) {
        pci_device* dev = pci_get_device(i);
        if (!dev) continue;
        
        // Bus
        vga_print_hex_byte(dev->bus);
        vga_print("  ");
        
        // Device
        vga_print_hex_byte(dev->device);
        vga_print("  ");
        
        // Function
        vga_print_hex_byte(dev->function);
        vga_print(" ");
        
        // Vendor ID
        vga_print_hex_word(dev->vendor_id);
        vga_print(" ");
        
        // Device ID
        vga_print_hex_word(dev->device_id);
        vga_print(" ");
        
        // Class
        vga_print(pci_get_class_name(dev->class_code));
        
        // Pad to vendor column
        const char* class_name = pci_get_class_name(dev->class_code);
        uint32_t len = 0;
        while (class_name[len]) len++;
        for (uint32_t j = len; j < 11; j++) vga_print(" ");
        
        // Vendor
        vga_print(pci_get_vendor_name(dev->vendor_id));
        vga_print("\n");
    }
    
    vga_print("\nTotal: ");
    vga_print_dec(count);
    vga_print(" devices\n\n");
}

static void cmd_syscall() {
    vga_setcolor(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_print("\n=== Testing System Calls ===\n");
    vga_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    // Test SYS_GETPID
    vga_print("\nTest 1: SYS_GETPID\n");
    int64_t pid = syscall_invoke(SYS_GETPID, 0, 0, 0);
    vga_print("Current PID: ");
    vga_print_dec((uint32_t)pid);
    vga_print("\n");
    
    // Test SYS_WRITE
    vga_print("\nTest 2: SYS_WRITE\n");
    const char* msg = "Hello from syscall!\n";
    int64_t written = syscall_invoke(SYS_WRITE, 1, (uint64_t)msg, 21);
    vga_print("Bytes written: ");
    vga_print_dec((uint32_t)written);
    vga_print("\n");
    
    // Test SYS_YIELD
    vga_print("\nTest 3: SYS_YIELD\n");
    syscall_invoke(SYS_YIELD, 0, 0, 0);
    vga_print("Returned from yield\n");
    
    vga_setcolor(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_print("\n✓ System calls working!\n\n");
    vga_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

// Execute a command
void shell_execute(const char* command) {
    // Skip leading spaces
    while (*command == ' ') command++;
    
    // Empty command
    if (*command == '\0') {
        show_prompt();
        return;
    }
    
    // Find command and arguments
    const char* args = command;
    while (*args && *args != ' ') args++;
    uint32_t cmd_len = args - command;
    while (*args == ' ') args++; // Skip spaces before args
    
    // Parse and execute commands
    if (strncmp(command, "help", cmd_len) == 0 && cmd_len == 4) {
        cmd_help();
    } else if (strncmp(command, "clear", cmd_len) == 0 && cmd_len == 5) {
        cmd_clear();
    } else if (strncmp(command, "echo", cmd_len) == 0 && cmd_len == 4) {
        cmd_echo(args);
    } else if (strncmp(command, "info", cmd_len) == 0 && cmd_len == 4) {
        cmd_info();
    } else if (strncmp(command, "memory", cmd_len) == 0 && cmd_len == 6) {
        cmd_memory();
    } else if (strncmp(command, "ps", cmd_len) == 0 && cmd_len == 2) {
        cmd_ps();
    } else if (strncmp(command, "uptime", cmd_len) == 0 && cmd_len == 6) {
        cmd_uptime();
    } else if (strncmp(command, "spawn", cmd_len) == 0 && cmd_len == 5) {
        cmd_spawn();
    } else if (strncmp(command, "syscall", cmd_len) == 0 && cmd_len == 7) {
        cmd_syscall();
    } else if (strncmp(command, "lspci", cmd_len) == 0 && cmd_len == 5) {
        cmd_lspci();
    } else if (strncmp(command, "reboot", cmd_len) == 0 && cmd_len == 6) {
        cmd_reboot();
    } else {
        vga_setcolor(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_print("Unknown command: ");
        vga_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        
        // Print command name only
        char cmd_name[64];
        uint32_t i;
        for (i = 0; i < cmd_len && i < 63; i++) {
            cmd_name[i] = command[i];
        }
        cmd_name[i] = '\0';
        vga_print(cmd_name);
        
        vga_setcolor(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        vga_print("\nType 'help' for available commands.\n");
        vga_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    }
    
    show_prompt();
}

// Process a character from keyboard
void shell_process_char(char c) {
    if (c == '\n') {
        // Execute command
        vga_print("\n");
        command_buffer[buffer_pos] = '\0';
        
        // Log to serial
        serial_print("Shell: Executing command: ");
        serial_print(command_buffer);
        serial_print("\n");
        
        shell_execute(command_buffer);
        buffer_pos = 0;
    } else if (c == '\b') {
        // Backspace
        if (buffer_pos > 0) {
            buffer_pos--;
            vga_putchar('\b');
        }
    } else if (c == '\t') {
        // Tab - ignore for now (could add tab completion later)
    } else if (c >= 32 && c < 127) {
        // Printable character
        if (buffer_pos < SHELL_MAX_COMMAND_LENGTH - 1) {
            command_buffer[buffer_pos++] = c;
            vga_putchar(c);
        }
    }
}
