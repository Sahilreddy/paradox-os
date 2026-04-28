// Linear framebuffer driver. Owns a back buffer; everything draws into the
// back buffer and `fb_present()` blits dirty regions to the hardware.

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "types.h"

// 0x00RRGGBB — alpha is unused (no transparency yet).
typedef uint32_t fb_color;

#define FB_RGB(r, g, b) (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

struct fb_rect {
    int32_t x, y;
    int32_t w, h;
};

struct fb_info {
    uint32_t* hw;       // hardware framebuffer (physical, identity-mapped)
    uint32_t* back;     // back buffer (heap)
    uint32_t  width;
    uint32_t  height;
    uint32_t  pitch_px; // hw stride in pixels
    bool      ready;
};

// Initialize from Multiboot2 framebuffer tag info. Returns false if the tag
// is missing or describes something other than 32-bpp RGB (the only mode
// we currently support).
bool fb_init(uint64_t addr, uint32_t pitch, uint32_t width, uint32_t height,
             uint8_t bpp, uint8_t fb_type);

bool fb_ready();
uint32_t fb_width();
uint32_t fb_height();

void fb_clear(fb_color c);
void fb_put_pixel(int32_t x, int32_t y, fb_color c);
void fb_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, fb_color c);
void fb_rect_outline(int32_t x, int32_t y, int32_t w, int32_t h, fb_color c);

// Blit a 32-bpp ARGB-style buffer (alpha ignored, treat 0xFF00FF as transparent
// for the cursor). `src_pitch_px` is the stride of `src` in pixels.
void fb_blit_keyed(const uint32_t* src, int32_t src_pitch_px,
                   int32_t dst_x, int32_t dst_y, int32_t w, int32_t h,
                   uint32_t key);

// Push the back buffer to the hardware framebuffer (full-frame).
void fb_present();

#endif
