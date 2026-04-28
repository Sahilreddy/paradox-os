// Linear-framebuffer driver. Owns a back buffer; everything draws into the
// back buffer and `fb_present()` blits the whole frame to the hardware.
//
// Why a back buffer at all? Even at 1024x768x32 we can repaint the whole
// screen in a fraction of a frame, but cursor + window dirtying produces
// visible tearing if we draw straight to VRAM. Compositing into RAM and
// blasting once per event keeps things tidy.

#include "../include/framebuffer.h"
#include "../include/memory.h"
#include "../include/serial.h"

static fb_info g_fb = { 0, 0, 0, 0, 0, false };

// 32-bpp word-stride blit / fill helpers. Always operate on the back buffer.
static inline uint32_t* back_row(int32_t y) {
    return g_fb.back + (uint32_t)y * g_fb.width;
}

bool fb_init(uint64_t addr, uint32_t pitch, uint32_t width, uint32_t height,
             uint8_t bpp, uint8_t fb_type) {
    if (fb_type != 1 /* RGB */ || bpp != 32) {
        serial_print("FB: unsupported framebuffer format (need RGB 32bpp)\n");
        return false;
    }

    g_fb.hw       = (uint32_t*)addr;
    g_fb.width    = width;
    g_fb.height   = height;
    g_fb.pitch_px = pitch / 4;

    // The back buffer is plain RAM; kmalloc is contiguous (single block out
    // of a contiguous heap), so a stride of `width` pixels is fine.
    uint64_t bytes = (uint64_t)width * height * 4;
    g_fb.back = (uint32_t*)kmalloc(bytes);
    if (!g_fb.back) {
        serial_print("FB: ERROR - failed to allocate back buffer\n");
        return false;
    }

    g_fb.ready = true;
    serial_print("FB: initialized ");
    serial_print_hex(width);
    serial_print("x");
    serial_print_hex(height);
    serial_print(" @ 0x");
    serial_print_hex((uint32_t)(addr >> 32));
    serial_print_hex((uint32_t)addr);
    serial_print("\n");
    return true;
}

bool     fb_ready()  { return g_fb.ready; }
uint32_t fb_width()  { return g_fb.width; }
uint32_t fb_height() { return g_fb.height; }

void fb_clear(fb_color c) {
    if (!g_fb.ready) return;
    uint32_t total = g_fb.width * g_fb.height;
    uint32_t* p = g_fb.back;
    for (uint32_t i = 0; i < total; i++) p[i] = c;
}

void fb_put_pixel(int32_t x, int32_t y, fb_color c) {
    if (!g_fb.ready) return;
    if (x < 0 || y < 0 || (uint32_t)x >= g_fb.width || (uint32_t)y >= g_fb.height) return;
    back_row(y)[x] = c;
}

void fb_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, fb_color c) {
    if (!g_fb.ready) return;
    // Clip to screen bounds.
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (w <= 0 || h <= 0) return;
    if ((uint32_t)x >= g_fb.width || (uint32_t)y >= g_fb.height) return;
    if ((uint32_t)(x + w) > g_fb.width)  w = g_fb.width  - x;
    if ((uint32_t)(y + h) > g_fb.height) h = g_fb.height - y;

    for (int32_t row = 0; row < h; row++) {
        uint32_t* line = back_row(y + row) + x;
        for (int32_t col = 0; col < w; col++) line[col] = c;
    }
}

void fb_rect_outline(int32_t x, int32_t y, int32_t w, int32_t h, fb_color c) {
    if (w <= 0 || h <= 0) return;
    fb_fill_rect(x,         y,         w, 1, c);
    fb_fill_rect(x,         y + h - 1, w, 1, c);
    fb_fill_rect(x,         y,         1, h, c);
    fb_fill_rect(x + w - 1, y,         1, h, c);
}

void fb_blit_keyed(const uint32_t* src, int32_t src_pitch_px,
                   int32_t dst_x, int32_t dst_y, int32_t w, int32_t h,
                   uint32_t key) {
    if (!g_fb.ready) return;
    // Clip the source rect against the screen, adjusting the source origin
    // so the region we copy stays in sync.
    int32_t sx = 0, sy = 0;
    if (dst_x < 0) { sx -= dst_x; w += dst_x; dst_x = 0; }
    if (dst_y < 0) { sy -= dst_y; h += dst_y; dst_y = 0; }
    if (w <= 0 || h <= 0) return;
    if ((uint32_t)dst_x >= g_fb.width || (uint32_t)dst_y >= g_fb.height) return;
    if ((uint32_t)(dst_x + w) > g_fb.width)  w = g_fb.width  - dst_x;
    if ((uint32_t)(dst_y + h) > g_fb.height) h = g_fb.height - dst_y;

    for (int32_t row = 0; row < h; row++) {
        const uint32_t* s = src + (sy + row) * src_pitch_px + sx;
        uint32_t* d = back_row(dst_y + row) + dst_x;
        for (int32_t col = 0; col < w; col++) {
            uint32_t px = s[col];
            if (px != key) d[col] = px;
        }
    }
}

void fb_present() {
    if (!g_fb.ready) return;
    // Hardware pitch in pixels can differ from `width`; copy line-by-line.
    for (uint32_t y = 0; y < g_fb.height; y++) {
        uint32_t* dst = g_fb.hw + y * g_fb.pitch_px;
        uint32_t* src = g_fb.back + y * g_fb.width;
        for (uint32_t x = 0; x < g_fb.width; x++) dst[x] = src[x];
    }
}
