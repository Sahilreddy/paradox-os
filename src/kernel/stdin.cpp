#include "../include/stdin.h"
#include "../include/vga.h"

#define LINE_CAP 256

static volatile char     g_line[LINE_CAP];
static volatile uint32_t g_len   = 0;
static volatile bool     g_ready = false;

void stdin_init() {
    g_len   = 0;
    g_ready = false;
}

void stdin_push(char c) {
    if (g_ready) return;

    if (c == '\b') {
        if (g_len > 0) {
            g_len--;
            vga_putchar('\b');
            vga_putchar(' ');
            vga_putchar('\b');
        }
        return;
    }

    if (c == '\n' || c == '\r') {
        if (g_len < LINE_CAP) g_line[g_len++] = '\n';
        vga_putchar('\n');
        g_ready = true;
        return;
    }

    if (g_len + 1 < LINE_CAP) {
        g_line[g_len++] = c;
        vga_putchar(c);
    }
}

uint64_t stdin_read_line(char* out, uint64_t cap) {
    // sti+hlt is atomic — sti's one-instruction shadow keeps the IRQ
    // from firing between them.
    while (!g_ready) asm volatile("sti; hlt");
    asm volatile("cli");

    uint64_t n = g_len;
    if (n > cap) n = cap;
    for (uint64_t i = 0; i < n; i++) out[i] = g_line[i];

    g_len   = 0;
    g_ready = false;
    return n;
}
