#include "../include/shell.h"
#include "../include/vga.h"
#include "../include/serial.h"
#include "../include/keyboard.h"
#include "../include/memory.h"
#include "../include/process.h"
#include "../include/timer.h"
#include "../include/syscall.h"
#include "../include/pci.h"
#include "../include/vfs.h"

static vfs_node* g_cwd = nullptr;

static vfs_node* shell_cwd() {
    if (!g_cwd) g_cwd = vfs_root();
    return g_cwd;
}

static char command_buffer[SHELL_MAX_COMMAND_LENGTH];
static uint16_t buffer_pos = 0;

static uint32_t strlen(const char* str) {
    uint32_t len = 0;
    while (str[len]) len++;
    return len;
}

static int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

static int strncmp(const char* s1, const char* s2, uint32_t n) {
    while (n && *s1 && (*s1 == *s2)) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

static void strcpy(char* dest, const char* src) {
    while (*src) *dest++ = *src++;
    *dest = '\0';
}

static void show_prompt() {
    vga_setcolor(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_print(SHELL_PROMPT);
    vga_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

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
    uint64_t seconds = ticks / 100;
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
    // pmm_get_total_memory sums every mmap region (BIOS holes included) so
    // it reads ~12 GB on a 512 MB VM. Use the managed total instead.
    uint64_t total = pmm_get_managed_memory();
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

    // KBC pulse-reset.
    uint8_t good = 0x02;
    while (good & 0x02) asm volatile("inb $0x64, %0" : "=a"(good));
    asm volatile("outb %0, $0x64" : : "a"((uint8_t)0xFE));

    while (1) asm("hlt");
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

        vga_print_hex_byte(dev->bus);       vga_print("  ");
        vga_print_hex_byte(dev->device);    vga_print("  ");
        vga_print_hex_byte(dev->function);  vga_print(" ");
        vga_print_hex_word(dev->vendor_id); vga_print(" ");
        vga_print_hex_word(dev->device_id); vga_print(" ");
        vga_print(pci_get_class_name(dev->class_code));

        const char* class_name = pci_get_class_name(dev->class_code);
        uint32_t len = 0; while (class_name[len]) len++;
        for (uint32_t j = len; j < 11; j++) vga_print(" ");

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

    vga_print("\nTest 2: SYS_WRITE\n");
    const char* msg = "Hello from syscall!\n";
    int64_t written = syscall_invoke(SYS_WRITE, 1, (uint64_t)msg, 21);
    vga_print("Bytes written: ");
    vga_print_dec((uint32_t)written);
    vga_print("\n");

    vga_print("\nTest 3: SYS_YIELD\n");
    syscall_invoke(SYS_YIELD, 0, 0, 0);
    vga_print("Returned from yield\n");

    vga_setcolor(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_print("\n* System calls working!\n\n");
    vga_setcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

static void cmd_pwd() {
    char path[128];
    vfs_path_of(shell_cwd(), path, sizeof(path));
    vga_print(path);
    vga_print("\n");
}

static void cmd_ls(const char* args) {
    vfs_node* dir = shell_cwd();
    if (args && *args) {
        vfs_node* target = vfs_lookup(args);
        if (target) dir = target;
        else { vga_print("ls: no such path: "); vga_print(args); vga_print("\n"); return; }
    }
    if (dir->kind != VFS_DIR) { vga_print(dir->name); vga_print("\n"); return; }
    for (vfs_node* c = dir->first_child; c; c = c->next) {
        const char* kind = c->kind == VFS_DIR ? "DIR "
                          : c->kind == VFS_EXEC ? "EXEC"
                          : "FILE";
        vga_print(kind);
        vga_print("  ");
        vga_print(c->name);
        if (c->kind == VFS_EXEC && c->description) {
            vga_print("    ");
            vga_print(c->description);
        }
        vga_print("\n");
    }
}

static void cmd_cat(const char* args) {
    if (!args || !*args) { vga_print("usage: cat <path>\n"); return; }
    vfs_node* n = vfs_lookup(args);
    if (!n) { vga_print("cat: no such file: "); vga_print(args); vga_print("\n"); return; }
    if (n->kind != VFS_FILE) { vga_print("cat: not a regular file\n"); return; }
    for (uint32_t i = 0; i < n->len; i++) vga_putchar(n->content[i]);
    if (n->len > 0 && n->content[n->len - 1] != '\n') vga_putchar('\n');
}

static void cmd_cd(const char* args) {
    if (!args || !*args || (args[0] == '/' && args[1] == 0)) {
        g_cwd = vfs_root();
        return;
    }
    if (args[0] == '.' && args[1] == '.' && (args[2] == 0 || args[2] == '/')) {
        if (g_cwd && g_cwd->parent) g_cwd = g_cwd->parent;
        return;
    }
    vfs_node* target = nullptr;
    if (args[0] == '/') {
        target = vfs_lookup(args);
    } else {
        char buf[160];
        vfs_path_of(shell_cwd(), buf, sizeof(buf));
        uint32_t i = 0; while (buf[i]) i++;
        if (i == 0 || buf[i - 1] != '/') { if (i + 1 < sizeof(buf)) buf[i++] = '/'; }
        for (uint32_t j = 0; args[j] && i + 1 < sizeof(buf); j++) buf[i++] = args[j];
        buf[i] = 0;
        target = vfs_lookup(buf);
    }
    if (!target)             { vga_print("cd: no such path: "); vga_print(args); vga_print("\n"); return; }
    if (target->kind != VFS_DIR) { vga_print("cd: not a directory\n"); return; }
    g_cwd = target;
}

// Forward declaration so cmd_sh below can recurse into shell_execute.
static void shell_execute_no_prompt(const char* command);

static void cmd_sh(const char* args) {
    if (!args || !*args) { vga_print("usage: sh <script-path>\n"); return; }
    vfs_node* n = vfs_lookup(args);
    if (!n || n->kind != VFS_FILE) {
        vga_print("sh: no such script: "); vga_print(args); vga_print("\n");
        return;
    }

    vga_print("[sh] "); vga_print(args); vga_print("\n");

    char line[SHELL_MAX_COMMAND_LENGTH];
    uint32_t li = 0;
    for (uint32_t i = 0; i <= n->len; i++) {
        char c = (i < n->len) ? n->content[i] : '\n';
        if (c == '\n' || c == '\r') {
            line[li] = 0;
            const char* p = line;
            while (*p == ' ' || *p == '\t') p++;
            if (*p && *p != '#') {
                vga_print(SHELL_PROMPT);
                vga_print(line);
                vga_print("\n");
                shell_execute_no_prompt(line);
            }
            li = 0;
        } else if (li + 1 < sizeof(line)) {
            line[li++] = c;
        }
    }
}

static void shell_execute_no_prompt(const char* command) {
    while (*command == ' ') command++;
    if (*command == '\0') return;

    const char* args = command;
    while (*args && *args != ' ') args++;
    uint32_t cmd_len = args - command;
    while (*args == ' ') args++;

    #define IS(s, n) (cmd_len == n && strncmp(command, s, n) == 0)
    if      (IS("help",    4)) cmd_help();
    else if (IS("clear",   5)) cmd_clear();
    else if (IS("echo",    4)) cmd_echo(args);
    else if (IS("info",    4)) cmd_info();
    else if (IS("memory",  6)) cmd_memory();
    else if (IS("ps",      2)) cmd_ps();
    else if (IS("uptime",  6)) cmd_uptime();
    else if (IS("spawn",   5)) cmd_spawn();
    else if (IS("syscall", 7)) cmd_syscall();
    else if (IS("lspci",   5)) cmd_lspci();
    else if (IS("reboot",  6)) cmd_reboot();
    else if (IS("pwd",     3)) cmd_pwd();
    else if (IS("ls",      2)) cmd_ls(args);
    else if (IS("cat",     3)) cmd_cat(args);
    else if (IS("cd",      2)) cmd_cd(args);
    else if (IS("sh",      2)) cmd_sh(args);
    #undef IS
    else {
        // Absolute -> use as-is; bare name -> /bin/<name>.
        char path[80];
        uint32_t pi = 0;
        if (command[0] == '/') {
            for (uint32_t i = 0; i < cmd_len && pi + 1 < sizeof(path); i++)
                path[pi++] = command[i];
        } else {
            const char* prefix = "/bin/";
            while (*prefix && pi + 1 < sizeof(path)) path[pi++] = *prefix++;
            for (uint32_t i = 0; i < cmd_len && pi + 1 < sizeof(path); i++)
                path[pi++] = command[i];
        }
        path[pi] = 0;
        vfs_node* n = vfs_lookup(path);
        if (n && n->kind == VFS_EXEC) {
            vfs_run(n, args);
        } else {
            vga_print("Unknown command: ");
            char nm[64]; uint32_t i;
            for (i = 0; i < cmd_len && i < 63; i++) nm[i] = command[i];
            nm[i] = 0;
            vga_print(nm);
            vga_print("\nType 'help' for built-ins, 'ls /bin' for programs.\n");
        }
    }
}

void shell_execute(const char* command) {
    shell_execute_no_prompt(command);
    show_prompt();
}

void shell_process_char(char c) {
    if (c == '\n') {
        vga_print("\n");
        command_buffer[buffer_pos] = '\0';

        serial_print("Shell: ");
        serial_print(command_buffer);
        serial_print("\n");

        shell_execute(command_buffer);
        buffer_pos = 0;
    } else if (c == '\b') {
        if (buffer_pos > 0) {
            buffer_pos--;
            vga_putchar('\b');
        }
    } else if (c == '\t') {
        // tab completion: TODO
    } else if (c >= 32 && c < 127) {
        if (buffer_pos < SHELL_MAX_COMMAND_LENGTH - 1) {
            command_buffer[buffer_pos++] = c;
            vga_putchar(c);
        }
    }
}
