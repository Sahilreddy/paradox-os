#include "../include/vfs.h"
#include "../include/memory.h"
#include "../include/serial.h"

static vfs_node* g_root = nullptr;

static uint32_t str_len(const char* s) {
    uint32_t n = 0; while (s && s[n]) n++; return n;
}
static void str_copy_n(char* dst, const char* src, uint32_t cap) {
    uint32_t i = 0;
    for (; src && src[i] && i + 1 < cap; i++) dst[i] = src[i];
    dst[i] = 0;
}
static bool str_eq(const char* a, const char* b) {
    while (*a && *b) { if (*a != *b) return false; a++; b++; }
    return *a == *b;
}
static void mem_copy(char* dst, const char* src, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) dst[i] = src[i];
}

static vfs_node* alloc_node(const char* name, vfs_kind k, vfs_node* parent) {
    vfs_node* n = (vfs_node*)kmalloc(sizeof(vfs_node));
    if (!n) return nullptr;
    str_copy_n(n->name, name, sizeof(n->name));
    n->kind = k;
    n->parent = parent;
    n->first_child = nullptr;
    n->next = nullptr;
    n->content = nullptr;
    n->len = 0;
    n->cap = 0;
    n->exec_fn = nullptr;
    n->description = nullptr;
    if (parent) {
        // Append, not prepend — children appear in the UI in insertion order.
        if (!parent->first_child) parent->first_child = n;
        else {
            vfs_node* p = parent->first_child;
            while (p->next) p = p->next;
            p->next = n;
        }
    }
    return n;
}

static vfs_node* mkdir_at(vfs_node* parent, const char* name) {
    return alloc_node(name, VFS_DIR, parent);
}

static vfs_node* mkfile_at(vfs_node* parent, const char* name,
                           const char* initial) {
    vfs_node* n = alloc_node(name, VFS_FILE, parent);
    if (!n) return nullptr;
    if (initial && *initial) vfs_write(n, initial, str_len(initial));
    return n;
}

static vfs_node* mkexec_at(vfs_node* parent, const char* name,
                           vfs_exec_fn fn, const char* desc) {
    vfs_node* n = alloc_node(name, VFS_EXEC, parent);
    if (!n) return nullptr;
    n->exec_fn = fn;
    n->description = desc;
    return n;
}

#include "../include/vga.h"
#include "../include/timer.h"
#include "../include/memory.h"
#include "../include/usermode.h"
#include "../include/elf.h"

extern "C" const uint8_t _binary_build_user_hello_elf_start[];
extern "C" const uint8_t _binary_build_user_hello_elf_end[];
extern "C" const uint8_t _binary_build_user_echo_elf_start[];
extern "C" const uint8_t _binary_build_user_echo_elf_end[];
extern "C" const uint8_t _binary_build_user_cat_elf_start[];
extern "C" const uint8_t _binary_build_user_cat_elf_end[];

// Hand-rolled ring-3 demo: SYS_HELLO; SYS_EXIT(0); jmp $.
static const uint8_t user_hello_bin[] = {
    0x48, 0xc7, 0xc0, 0x0a, 0x00, 0x00, 0x00,
    0xcd, 0x80,
    0x48, 0xc7, 0xc0, 0x04, 0x00, 0x00, 0x00,
    0x48, 0x31, 0xff,
    0xcd, 0x80,
    0xeb, 0xfe
};

static void cmd_clear(const char* args) {
    (void)args;
    vga_print("\n--- terminal cleared ---\n\n");
}

static void cmd_hello(const char* args) {
    (void)args;

    uint8_t* stack = (uint8_t*)pmm_alloc_page();
    if (!stack) {
        vga_print("/bin/hello: out of memory for user stack\n");
        return;
    }
    for (int i = 0; i < 4096; i++) stack[i] = 0xCC;     // 0xCC = obvious in dumps

    vga_print("\n[/bin/hello] dropping to ring 3...\n");
    usermode_run((uint64_t)user_hello_bin, (uint64_t)stack + 4096);
    vga_print("[/bin/hello] returned from ring 3 cleanly.\n\n");

    pmm_free_page(stack);
}

static void cmd_uptime(const char* args) {
    (void)args;
    uint64_t s = timer_get_ticks() / 100;
    char line[80];
    uint32_t n = 0;
    static const char* prefix = "uptime: ";
    while (prefix[n]) { line[n] = prefix[n]; n++; }
    // hours
    char tmp[16]; int ti = 0;
    uint64_t v = s / 3600;
    if (v == 0) tmp[ti++] = '0';
    while (v) { tmp[ti++] = '0' + (v % 10); v /= 10; }
    while (ti > 0 && n < sizeof(line) - 1) line[n++] = tmp[--ti];
    line[n++] = 'h'; line[n++] = ' ';
    ti = 0; v = (s / 60) % 60;
    if (v == 0) tmp[ti++] = '0';
    while (v) { tmp[ti++] = '0' + (v % 10); v /= 10; }
    while (ti > 0 && n < sizeof(line) - 1) line[n++] = tmp[--ti];
    line[n++] = 'm'; line[n++] = ' ';
    ti = 0; v = s % 60;
    if (v == 0) tmp[ti++] = '0';
    while (v) { tmp[ti++] = '0' + (v % 10); v /= 10; }
    while (ti > 0 && n < sizeof(line) - 1) line[n++] = tmp[--ti];
    line[n++] = 's'; line[n++] = '\n'; line[n] = 0;
    vga_print(line);
}

static void run_user_elf(const uint8_t* start, const uint8_t* end,
                         const char* prog_name, const char* args) {
    uint32_t size  = (uint32_t)(end - start);
    uint64_t entry = elf_load(start, size);
    if (!entry) {
        vga_print("[");
        vga_print(prog_name);
        vga_print("] ELF load failed.\n");
        return;
    }
    vga_print("\n[");
    vga_print(prog_name);
    vga_print("] -> ring 3\n");
    usermode_run_with_args(entry, prog_name, args);
    vga_print("[");
    vga_print(prog_name);
    vga_print("] returned.\n\n");
}

static void cmd_elf_hello(const char* args) {
    run_user_elf(_binary_build_user_hello_elf_start,
                 _binary_build_user_hello_elf_end,
                 "/bin/elf-hello", args);
}

static void cmd_echo(const char* args) {
    run_user_elf(_binary_build_user_echo_elf_start,
                 _binary_build_user_echo_elf_end,
                 "/bin/echo", args);
}

static void cmd_cat(const char* args) {
    run_user_elf(_binary_build_user_cat_elf_start,
                 _binary_build_user_cat_elf_end,
                 "/bin/cat", args);
}

static void cmd_meminfo(const char* args) {
    (void)args;
    char line[80];
    uint32_t n = 0;
    static const char* prefix = "RAM free / total : ";
    while (prefix[n]) { line[n] = prefix[n]; n++; }
    uint64_t fr = pmm_get_free_memory() / (1024 * 1024);
    uint64_t tot = pmm_get_managed_memory() / (1024 * 1024);
    char tmp[16]; int ti = 0;
    if (fr == 0) tmp[ti++] = '0';
    while (fr) { tmp[ti++] = '0' + (fr % 10); fr /= 10; }
    while (ti > 0 && n < sizeof(line) - 1) line[n++] = tmp[--ti];
    line[n++] = ' '; line[n++] = '/'; line[n++] = ' ';
    ti = 0;
    if (tot == 0) tmp[ti++] = '0';
    while (tot) { tmp[ti++] = '0' + (tot % 10); tot /= 10; }
    while (ti > 0 && n < sizeof(line) - 1) line[n++] = tmp[--ti];
    line[n++] = ' '; line[n++] = 'M'; line[n++] = 'i'; line[n++] = 'B';
    line[n++] = '\n'; line[n] = 0;
    vga_print(line);
}

void vfs_init() {
    g_root = alloc_node("/", VFS_DIR, nullptr);
    if (!g_root) return;

    vfs_node* home = mkdir_at(g_root, "home");
    vfs_node* docs = mkdir_at(g_root, "docs");
    vfs_node* bin  = mkdir_at(g_root, "bin");

    mkfile_at(home, "welcome.txt",
        "Welcome to ParadoxOS!\n"
        "\n"
        "You're inside an in-memory filesystem. Click any file in the\n"
        "left tree to select it, then use Open or Edit at the top of\n"
        "the window. Edits persist until reboot.\n"
        "\n"
        "Try opening /bin/hello and clicking Run.\n");

    mkfile_at(home, "notes.txt",
        "scratchpad\n"
        "==========\n"
        "\n"
        "type stuff here.\n");

    mkfile_at(home, "demo.sh",
        "# ParadoxOS demo script.\n"
        "# Run with: sh /home/demo.sh   (in any Terminal window)\n"
        "\n"
        "echo ParadoxOS shell script demo\n"
        "echo ---------------------------\n"
        "pwd\n"
        "ls /bin\n"
        "uptime\n"
        "memory\n"
        "echo Now running a real ring-3 ELF from a script:\n"
        "/bin/echo hello from a ring-3 program launched by sh\n"
        "echo done.\n");

    mkfile_at(docs, "about.md",
        "# ParadoxOS\n"
        "\n"
        "A graphical hobby kernel built up in tracks:\n"
        "\n"
        " - Track A: framebuffer + compositor + desktop (done)\n"
        " - Track B: ring-3 userspace + ELF loader (in progress)\n"
        " - Track C: real disk + filesystem + network (in progress)\n");

    mkfile_at(docs, "track-roadmap.txt",
        "next steps:\n"
        "  - load TSS, set user bits, jump to ring 3\n"
        "  - tiny ELF loader\n"
        "  - FAT32 read on top of ATA PIO\n");

    mkexec_at(bin, "hello",     cmd_hello,
        "ring 3 demo (raw bytes, no argv)");
    mkexec_at(bin, "elf-hello", cmd_elf_hello,
        "ring 3 ELF demo (prints argv it received)");
    mkexec_at(bin, "echo",      cmd_echo,
        "echo args back to stdout");
    mkexec_at(bin, "cat",       cmd_cat,
        "read a line from stdin and echo it");
    mkexec_at(bin, "clear",     cmd_clear,
        "writes a clear banner into the terminal");
    mkexec_at(bin, "uptime",    cmd_uptime,
        "prints kernel uptime into the terminal");
    mkexec_at(bin, "meminfo",   cmd_meminfo,
        "prints free / total RAM into the terminal");

    serial_print("VFS: initialized\n");
}

vfs_node* vfs_root() { return g_root; }

vfs_node* vfs_lookup(const char* path) {
    if (!g_root || !path) return nullptr;
    if (path[0] != '/') return nullptr;
    if (path[1] == 0) return g_root;

    vfs_node* cur = g_root;
    const char* p = path + 1;
    while (*p) {
        // Read one path component.
        char comp[32]; uint32_t ci = 0;
        while (*p && *p != '/' && ci + 1 < sizeof(comp)) comp[ci++] = *p++;
        comp[ci] = 0;
        if (*p == '/') p++;

        if (cur->kind != VFS_DIR) return nullptr;
        vfs_node* child = cur->first_child;
        while (child && !str_eq(child->name, comp)) child = child->next;
        if (!child) return nullptr;
        cur = child;
    }
    return cur;
}

void vfs_path_of(const vfs_node* n, char* out, uint32_t cap) {
    if (!n || cap == 0) { if (cap) out[0] = 0; return; }
    if (n == g_root) {
        if (cap >= 2) { out[0] = '/'; out[1] = 0; } else out[0] = 0;
        return;
    }
    // Walk parents into a stack first, then write forward.
    const vfs_node* stack[16];
    int depth = 0;
    while (n && n != g_root && depth < 16) {
        stack[depth++] = n;
        n = n->parent;
    }
    uint32_t pos = 0;
    for (int i = depth - 1; i >= 0; i--) {
        if (pos + 1 >= cap) break;
        out[pos++] = '/';
        for (uint32_t j = 0; stack[i]->name[j] && pos + 1 < cap; j++)
            out[pos++] = stack[i]->name[j];
    }
    if (pos == 0 && cap >= 2) { out[pos++] = '/'; }
    out[pos] = 0;
}

static bool ensure_cap(vfs_node* f, uint32_t need) {
    if (f->cap >= need) return true;
    uint32_t newcap = f->cap == 0 ? 64 : f->cap;
    while (newcap < need) newcap *= 2;
    char* nb = (char*)kmalloc(newcap);
    if (!nb) return false;
    if (f->content) {
        for (uint32_t i = 0; i < f->len; i++) nb[i] = f->content[i];
        kfree(f->content);
    }
    f->content = nb;
    f->cap = newcap;
    return true;
}

bool vfs_write(vfs_node* file, const char* data, uint32_t len) {
    if (!file || file->kind != VFS_FILE) return false;
    if (!ensure_cap(file, len + 1)) return false;
    mem_copy(file->content, data, len);
    file->content[len] = 0;
    file->len = len;
    return true;
}

bool vfs_append_char(vfs_node* file, char c) {
    if (!file || file->kind != VFS_FILE) return false;
    if (!ensure_cap(file, file->len + 2)) return false;
    file->content[file->len++] = c;
    file->content[file->len] = 0;
    return true;
}

bool vfs_pop_char(vfs_node* file) {
    if (!file || file->kind != VFS_FILE || file->len == 0) return false;
    file->len--;
    if (file->content) file->content[file->len] = 0;
    return true;
}

void vfs_run(vfs_node* n, const char* args) {
    if (!n || n->kind != VFS_EXEC || !n->exec_fn) return;
    n->exec_fn(args);
}
