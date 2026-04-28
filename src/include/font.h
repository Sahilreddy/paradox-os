// 8x16 bitmap font + text rendering on the framebuffer back buffer.

#ifndef FONT_H_GUARD
#define FONT_H_GUARD

#include "types.h"
#include "framebuffer.h"

#define FONT_WIDTH  8
#define FONT_HEIGHT 16

// Render a single glyph. fg is opaque; if `bg_opaque` is false, background
// pixels are left untouched (useful for drawing text over an image).
void font_draw_char(int32_t x, int32_t y, char c,
                    fb_color fg, fb_color bg, bool bg_opaque);

// Render a NUL-terminated string. Newlines wrap to (start_x, y + FONT_HEIGHT).
// Returns the y coordinate of the next line.
int32_t font_draw_string(int32_t x, int32_t y, const char* s,
                         fb_color fg, fb_color bg, bool bg_opaque);

// Render a single line up to `n` chars (used by the shell terminal).
void font_draw_n(int32_t x, int32_t y, const char* s, uint32_t n,
                 fb_color fg, fb_color bg, bool bg_opaque);

// Pixel-doubled rendering for big banners (splash logo, etc.). Scale 1
// matches normal rendering. Each lit bit becomes a `scale x scale` block.
// No anti-aliasing; the "chunky" look is intentional.
void font_draw_string_scaled(int32_t x, int32_t y, const char* s,
                             fb_color fg, int scale);

#endif
