// PS/2 mouse driver. Standard 3-byte packet protocol on the AUX channel
// (port 0x60 / 0x64), driven by IRQ12.
//
// Initialization sequence (cookbook):
//   1. Disable both PS/2 ports
//   2. Flush the output buffer
//   3. Enable IRQ12 in the controller config byte (also re-enable port 1 IRQ)
//   4. Re-enable both ports
//   5. Tell the AUX device: set defaults (0xF6), then enable streaming (0xF4)
//   6. Unmask IRQ12 + IRQ2 (cascade) on the PIC

#include "../include/mouse.h"
#include "../include/pic.h"
#include "../include/serial.h"

static inline void outb(uint16_t port, uint8_t v) {
    asm volatile("outb %0, %1" : : "a"(v), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t r;
    asm volatile("inb %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}

#define PS2_DATA   0x60
#define PS2_STATUS 0x64
#define PS2_CMD    0x64

static mouse_state g_mouse = { 0, 0, 0, false };
static int32_t g_screen_w = 0;
static int32_t g_screen_h = 0;

// Edge-event accumulators. We OR every press/release transition we see in
// the IRQ handler into these so `mouse_consume_event` doesn't lose clicks
// that happen entirely between two GUI ticks (down + up in <10ms).
static uint8_t g_irq_prev_buttons = 0;
static uint8_t g_pending_pressed  = 0;
static uint8_t g_pending_released = 0;

// 3-byte packet assembler.
static uint8_t packet[3];
static uint8_t packet_idx = 0;

static void wait_input_ready() {
    // bit 1 of status = input buffer full; we wait until clear.
    for (int i = 0; i < 100000; i++) {
        if ((inb(PS2_STATUS) & 0x02) == 0) return;
    }
}
static void wait_output_ready() {
    // bit 0 of status = output buffer full; we wait until set.
    for (int i = 0; i < 100000; i++) {
        if (inb(PS2_STATUS) & 0x01) return;
    }
}

static void mouse_write(uint8_t b) {
    wait_input_ready();
    outb(PS2_CMD, 0xD4);   // "next byte goes to AUX port"
    wait_input_ready();
    outb(PS2_DATA, b);
}

static uint8_t mouse_read() {
    wait_output_ready();
    return inb(PS2_DATA);
}

void mouse_init(int32_t screen_w, int32_t screen_h) {
    g_screen_w = screen_w;
    g_screen_h = screen_h;
    g_mouse.x = screen_w / 2;
    g_mouse.y = screen_h / 2;
    g_mouse.buttons = 0;
    g_mouse.moved = true;

    // 1. Disable both ports while we configure.
    wait_input_ready(); outb(PS2_CMD, 0xAD); // disable port 1
    wait_input_ready(); outb(PS2_CMD, 0xA7); // disable port 2

    // 2. Flush.
    while (inb(PS2_STATUS) & 0x01) (void)inb(PS2_DATA);

    // 3. Read current config, enable IRQ12 (bit 1) + IRQ1 (bit 0), clear
    //    "disable mouse clock" (bit 5).
    wait_input_ready(); outb(PS2_CMD, 0x20);
    uint8_t cfg = mouse_read();
    cfg |= (1 << 0) | (1 << 1);
    cfg &= ~(1 << 5);
    wait_input_ready(); outb(PS2_CMD, 0x60);
    wait_input_ready(); outb(PS2_DATA, cfg);

    // 4. Re-enable both ports.
    wait_input_ready(); outb(PS2_CMD, 0xAE);
    wait_input_ready(); outb(PS2_CMD, 0xA8);

    // 5. AUX device: defaults + enable streaming.
    mouse_write(0xF6); (void)mouse_read(); // ACK
    mouse_write(0xF4); (void)mouse_read(); // ACK

    // 6. Unmask IRQ12 (and the cascade IRQ2 to be safe).
    pic_clear_mask(2);
    pic_clear_mask(12);

    serial_print("Mouse: PS/2 mouse initialized\n");
}

void mouse_handler() {
    uint8_t status = inb(PS2_STATUS);
    if (!(status & 0x01) || !(status & 0x20)) {
        // Output buffer empty, or it's a keyboard byte. Just EOI.
        pic_send_eoi(12);
        return;
    }

    uint8_t b = inb(PS2_DATA);

    // First byte must have bit 3 (the "always 1" sync bit) set; if it doesn't,
    // we resync the packet stream.
    if (packet_idx == 0 && !(b & 0x08)) {
        pic_send_eoi(12);
        return;
    }

    packet[packet_idx++] = b;
    if (packet_idx < 3) {
        pic_send_eoi(12);
        return;
    }
    packet_idx = 0;

    uint8_t flags = packet[0];
    int32_t dx = packet[1];
    int32_t dy = packet[2];

    // Sign-extend using the X/Y sign bits in `flags`.
    if (flags & 0x10) dx |= 0xFFFFFF00;
    if (flags & 0x20) dy |= 0xFFFFFF00;

    // Drop packets that overflowed (X/Y overflow bits in flags).
    if (!(flags & 0xC0)) {
        g_mouse.x += dx;
        g_mouse.y -= dy; // PS/2 Y grows upward; screen Y grows downward.

        if (g_mouse.x < 0)             g_mouse.x = 0;
        if (g_mouse.y < 0)             g_mouse.y = 0;
        if (g_mouse.x >= g_screen_w)   g_mouse.x = g_screen_w - 1;
        if (g_mouse.y >= g_screen_h)   g_mouse.y = g_screen_h - 1;
    }

    uint8_t new_buttons = flags & 0x07;
    g_pending_pressed  |= (uint8_t)(new_buttons & ~g_irq_prev_buttons);
    g_pending_released |= (uint8_t)(g_irq_prev_buttons & ~new_buttons);
    g_irq_prev_buttons = new_buttons;

    g_mouse.buttons = new_buttons;
    g_mouse.moved = true;

    pic_send_eoi(12);
}

mouse_state mouse_consume() {
    mouse_state s = g_mouse;
    g_mouse.moved = false;
    return s;
}

const mouse_state* mouse_peek() { return &g_mouse; }

mouse_event mouse_consume_event() {
    mouse_event ev;
    ev.x = g_mouse.x;
    ev.y = g_mouse.y;
    ev.buttons  = g_mouse.buttons;
    // Atomically read-and-clear the accumulated edges so no click is lost
    // even when down/up happen entirely between two consume() calls.
    asm volatile("cli");
    ev.pressed  = g_pending_pressed;
    ev.released = g_pending_released;
    g_pending_pressed  = 0;
    g_pending_released = 0;
    asm volatile("sti");
    g_mouse.moved = false;
    return ev;
}
