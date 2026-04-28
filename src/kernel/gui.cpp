// Desktop compositor.
//
// Layout:
//   - Boot splash (gui_run_splash) draws a centered logo + animated
//     progress bar while we wait a couple of seconds. Blocks the caller.
//   - Desktop is a starfield over a flat color, a column of clickable
//     icons down the left edge, and a taskbar at the bottom showing
//     the brand and a live ticker clock.
//   - Each icon, when clicked, opens (or focuses) a window of one of
//     four app types: Terminal, Files, Monitor, About.
//   - Windows can be dragged by their titlebar, brought to the front
//     by clicking, and dismissed with the close button. The most-
//     recently-focused window has a brighter titlebar.
//   - The cursor is composited last on top of everything.
//
// Repainting is event-driven: gui_tick redraws when a click or drag
// changes state, when a redraw-needing event happens (term dirty, drag
// motion, clock tick), or when the cursor moved. At 1024x768x32 a full
// repaint is trivially cheap.

#include "../include/gui.h"
#include "../include/framebuffer.h"
#include "../include/font.h"
#include "../include/mouse.h"
#include "../include/serial.h"
#include "../include/timer.h"
#include "../include/memory.h"
#include "../include/pci.h"
#include "../include/ata.h"

// ----- Theme ----------------------------------------------------------------
static const fb_color C_DESKTOP    = FB_RGB(0x0a, 0x0e, 0x1c);
static const fb_color C_DESKTOP_2  = FB_RGB(0x12, 0x18, 0x2b);
static const fb_color C_TASKBAR    = FB_RGB(0x06, 0x09, 0x14);
static const fb_color C_TASKBAR_HI = FB_RGB(0x4a, 0x90, 0xe2);
static const fb_color C_WIN_BORDER = FB_RGB(0x3a, 0x4a, 0x6a);
static const fb_color C_WIN_TITLE  = FB_RGB(0x1f, 0x2a, 0x44);
static const fb_color C_WIN_TITLE_F= FB_RGB(0x2c, 0x44, 0x70);
static const fb_color C_WIN_BG     = FB_RGB(0x0d, 0x12, 0x1f);
static const fb_color C_TEXT       = FB_RGB(0xff, 0xff, 0xff);
static const fb_color C_TEXT_DIM   = FB_RGB(0x9f, 0xae, 0xc4);
static const fb_color C_TERM_FG    = FB_RGB(0xc8, 0xd6, 0xea);
static const fb_color C_TERM_PROMPT= FB_RGB(0x6a, 0xd2, 0x9a);
static const fb_color C_ICON_LABEL = FB_RGB(0xe8, 0xee, 0xfa);
static const fb_color C_ICON_HOVER = FB_RGB(0x4a, 0x90, 0xe2);
static const fb_color C_BAR_FILL   = FB_RGB(0x4a, 0xc8, 0x90);
static const fb_color C_BAR_USED   = FB_RGB(0xe2, 0x66, 0x66);

static const fb_color C_TERM_BODY    = FB_RGB(0x1a, 0x22, 0x36);
static const fb_color C_TERM_BORDER  = FB_RGB(0x55, 0xaa, 0x88);
static const fb_color C_FILES_BODY   = FB_RGB(0xe6, 0xc8, 0x6a);
static const fb_color C_FILES_TAB    = FB_RGB(0xc7, 0xa1, 0x40);
static const fb_color C_ABOUT_BODY   = FB_RGB(0x4a, 0x90, 0xe2);
static const fb_color C_ABOUT_HI     = FB_RGB(0xff, 0xff, 0xff);
static const fb_color C_MONITOR_BODY = FB_RGB(0x6a, 0xd2, 0x9a);
static const fb_color C_MONITOR_HI   = FB_RGB(0x10, 0x30, 0x20);

static const fb_color C_CURSOR_KEY = FB_RGB(0xff, 0x00, 0xff);

// ----- Mouse cursor sprite --------------------------------------------------
#define CURSOR_W 12
#define CURSOR_H 19
static const char* cursor_pixels[CURSOR_H] = {
    "Bkkkkkkkkkkk", "BBkkkkkkkkkk", "BWBkkkkkkkkk", "BWWBkkkkkkkk",
    "BWWWBkkkkkkk", "BWWWWBkkkkkk", "BWWWWWBkkkkk", "BWWWWWWBkkkk",
    "BWWWWWWWBkkk", "BWWWWWWWWBkk", "BWWWWWWWWWBk", "BWWWWWBBBBBB",
    "BWWBWWBkkkkk", "BWBkBWWBkkkk", "BBkkBWWBkkkk", "Bkkkk BWWBkk",
    "kkkkk BWWBkk", "kkkkkk BWWBk", "kkkkkk BBBBk",
};
static uint32_t cursor_buf[CURSOR_W * CURSOR_H];

// ----- Persistent terminal grid --------------------------------------------
#define TERM_COLS 96
#define TERM_ROWS 36
struct term_state {
    char     cells[TERM_ROWS][TERM_COLS];
    int32_t  cur_row;
    int32_t  cur_col;
    bool     dirty;
};
static term_state term;

// ----- Apps + windows -------------------------------------------------------
enum app_type {
    APP_TERMINAL = 0,
    APP_FILES    = 1,
    APP_MONITOR  = 2,
    APP_ABOUT    = 3,
};

#define MAX_WINDOWS 8
struct window_t {
    bool      used;
    int32_t   x, y, w, h;
    app_type  type;
    const char* title;
};
static window_t  windows[MAX_WINDOWS];
static int       z_order[MAX_WINDOWS];
static int       z_count = 0;

static int       g_next_cascade = 0;

#define TITLE_H     24
#define WIN_PAD     8
#define CLOSE_W     12
#define CLOSE_H     12
#define CLOSE_INSET 10

// Drag state.
static int     g_drag_idx = -1;   // index of window being dragged, or -1
static int32_t g_drag_off_x = 0;  // offset from cursor to window origin
static int32_t g_drag_off_y = 0;

// ----- Desktop icons --------------------------------------------------------
struct icon {
    int32_t  x, y;
    const char* label;
    app_type app;
};
#define ICON_W 80
#define ICON_H 80
#define ICON_LABEL_GAP 4

#define N_ICONS 4
static icon icons[N_ICONS] = {
    { 30,  40,  "Terminal", APP_TERMINAL },
    { 30,  150, "Files",    APP_FILES    },
    { 30,  260, "Monitor",  APP_MONITOR  },
    { 30,  370, "About",    APP_ABOUT    },
};

// ----- Boot disk snapshot --------------------------------------------------
// Read once at gui_init time. Lets the Files app show real bytes from
// disk without doing PIO on every redraw.
static uint8_t g_boot_sector[512];
static bool    g_boot_sector_valid = false;

// ----- Helpers --------------------------------------------------------------
static uint32_t kstrlen(const char* s) {
    uint32_t n = 0; while (s[n]) n++; return n;
}

static void cursor_compile() {
    for (int y = 0; y < CURSOR_H; y++) {
        for (int x = 0; x < CURSOR_W; x++) {
            char c = cursor_pixels[y][x];
            uint32_t px = C_CURSOR_KEY;
            if (c == 'B') px = FB_RGB(0x00, 0x00, 0x00);
            else if (c == 'W') px = FB_RGB(0xff, 0xff, 0xff);
            cursor_buf[y * CURSOR_W + x] = px;
        }
    }
}

static void term_clear() {
    for (int r = 0; r < TERM_ROWS; r++)
        for (int c = 0; c < TERM_COLS; c++)
            term.cells[r][c] = ' ';
    term.cur_row = 0;
    term.cur_col = 0;
    term.dirty = true;
}

static void term_scroll_one() {
    for (int r = 0; r < TERM_ROWS - 1; r++)
        for (int c = 0; c < TERM_COLS; c++)
            term.cells[r][c] = term.cells[r + 1][c];
    for (int c = 0; c < TERM_COLS; c++)
        term.cells[TERM_ROWS - 1][c] = ' ';
}

static void term_newline() {
    term.cur_col = 0;
    term.cur_row++;
    if (term.cur_row >= TERM_ROWS) {
        term_scroll_one();
        term.cur_row = TERM_ROWS - 1;
    }
}

// ----- Tiny formatting ------------------------------------------------------
static uint32_t fmt_u64(char* out, uint32_t cap, uint64_t v) {
    char buf[24]; int i = 0;
    if (v == 0) buf[i++] = '0';
    while (v > 0) { buf[i++] = '0' + (v % 10); v /= 10; }
    uint32_t n = 0;
    while (i > 0 && n + 1 < cap) out[n++] = buf[--i];
    out[n] = 0;
    return n;
}

static uint32_t fmt_hex(char* out, uint32_t cap, uint64_t v, int digits) {
    static const char* H = "0123456789ABCDEF";
    uint32_t n = 0;
    if (n + 2 < cap) { out[n++] = '0'; out[n++] = 'x'; }
    for (int s = (digits - 1) * 4; s >= 0 && n + 1 < cap; s -= 4)
        out[n++] = H[(v >> s) & 0xF];
    out[n] = 0;
    return n;
}

static uint32_t cat(char* dst, uint32_t cap, uint32_t pos, const char* src) {
    while (*src && pos + 1 < cap) dst[pos++] = *src++;
    dst[pos] = 0;
    return pos;
}

// ----- Z-order management ---------------------------------------------------
static void z_bring_to_top(int idx) {
    int pos = -1;
    for (int i = 0; i < z_count; i++) if (z_order[i] == idx) { pos = i; break; }
    if (pos < 0) return;
    for (int i = pos; i < z_count - 1; i++) z_order[i] = z_order[i + 1];
    z_order[z_count - 1] = idx;
}

static void z_remove(int idx) {
    int pos = -1;
    for (int i = 0; i < z_count; i++) if (z_order[i] == idx) { pos = i; break; }
    if (pos < 0) return;
    for (int i = pos; i < z_count - 1; i++) z_order[i] = z_order[i + 1];
    z_count--;
}

static int find_window_of_type(app_type t) {
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (windows[i].used && windows[i].type == t) return i;
    return -1;
}

static int alloc_window() {
    for (int i = 0; i < MAX_WINDOWS; i++)
        if (!windows[i].used) return i;
    return -1;
}

static const char* title_for(app_type t) {
    switch (t) {
        case APP_TERMINAL: return "Terminal";
        case APP_FILES:    return "Files - System Browser";
        case APP_MONITOR:  return "System Monitor";
        case APP_ABOUT:    return "About ParadoxOS";
    }
    return "";
}

static void open_app(app_type t) {
    int existing = find_window_of_type(t);
    if (existing >= 0) {
        z_bring_to_top(existing);
        term.dirty = true;
        return;
    }
    int idx = alloc_window();
    if (idx < 0) return;

    int32_t W = (int32_t)fb_width();
    int32_t H = (int32_t)fb_height();

    int32_t w = 480, h = 320;
    switch (t) {
        case APP_TERMINAL: w = TERM_COLS * FONT_WIDTH + 2 * WIN_PAD;
                           h = TERM_ROWS * FONT_HEIGHT + TITLE_H + 2 * WIN_PAD;
                           break;
        case APP_FILES:    w = 720; h = 540; break;
        case APP_MONITOR:  w = 520; h = 380; break;
        case APP_ABOUT:    w = 480; h = 320; break;
    }
    if (w > W - 40) w = W - 40;
    if (h > H - 80) h = H - 80;

    int32_t base_x = 160 + (g_next_cascade % 4) * 26;
    int32_t base_y = 60  + (g_next_cascade % 4) * 26;
    g_next_cascade++;
    if (base_x + w > W - 10) base_x = W - w - 10;
    if (base_y + h > H - 50) base_y = H - h - 50;

    windows[idx].used  = true;
    windows[idx].x = base_x;
    windows[idx].y = base_y;
    windows[idx].w = w;
    windows[idx].h = h;
    windows[idx].type  = t;
    windows[idx].title = title_for(t);

    z_order[z_count++] = idx;
    term.dirty = true;
}

static void close_window(int idx) {
    if (idx < 0 || idx >= MAX_WINDOWS || !windows[idx].used) return;
    windows[idx].used = false;
    z_remove(idx);
    term.dirty = true;
}

// ----- Drawing primitives ---------------------------------------------------

// Animated starfield. Each star's position is computed deterministically
// from its index + a tick offset, so we don't need to maintain mutable
// state — and the result still looks alive.
static void draw_starfield() {
    uint64_t t = timer_get_ticks();
    const int N_STARS = 110;
    for (int i = 0; i < N_STARS; i++) {
        uint32_t px = 0xC0DEF00D ^ (i * 0x9E3779B9u);
        uint32_t py = 0xDEADBEEF ^ (i * 0x85EBCA6Bu);
        uint32_t speed = 1 + (px % 4); // 1..4 px per tick chunk
        int32_t x = (int32_t)((px ^ (uint32_t)(t * speed >> 4)) % fb_width());
        int32_t y = (int32_t)(py % fb_height());
        uint8_t bri = 60 + (uint8_t)(px % 180);
        fb_color c = FB_RGB(bri, bri, (uint8_t)(bri | 40));
        fb_put_pixel(x, y, c);
        // Streak the brighter ones for a "warp" feel.
        if (bri > 200) {
            fb_put_pixel(x - 1, y, FB_RGB(bri / 2, bri / 2, bri / 2));
            fb_put_pixel(x - 2, y, FB_RGB(bri / 4, bri / 4, bri / 4));
        }
    }
}

static void draw_desktop_bg() {
    // Vertical-gradient-ish background by alternating two close colors.
    uint32_t H = fb_height();
    for (uint32_t y = 0; y < H; y++) {
        fb_color c = (y / 4) & 1 ? C_DESKTOP : C_DESKTOP_2;
        fb_fill_rect(0, y, fb_width(), 1, c);
    }
    draw_starfield();

    // Taskbar
    int32_t bar_h = 32;
    fb_fill_rect(0, fb_height() - bar_h, fb_width(), bar_h, C_TASKBAR);
    fb_fill_rect(0, fb_height() - bar_h, fb_width(), 1, C_TASKBAR_HI);
    font_draw_string(10, fb_height() - bar_h + (bar_h - FONT_HEIGHT) / 2,
                     "ParadoxOS", C_TASKBAR_HI, 0, false);

    // Right side: tick-based clock.
    char buf[32];
    uint64_t s = timer_get_ticks() / 100;
    uint32_t hh = (uint32_t)(s / 3600);
    uint32_t mm = (uint32_t)((s / 60) % 60);
    uint32_t ss = (uint32_t)(s % 60);
    uint32_t n = 0;
    n += fmt_u64(buf + n, sizeof(buf) - n, hh);
    buf[n++] = ':'; if (mm < 10) buf[n++] = '0';
    n += fmt_u64(buf + n, sizeof(buf) - n, mm);
    buf[n++] = ':'; if (ss < 10) buf[n++] = '0';
    n += fmt_u64(buf + n, sizeof(buf) - n, ss);
    buf[n] = 0;
    int32_t tx = (int32_t)fb_width() - (int32_t)n * FONT_WIDTH - 12;
    font_draw_string(tx, fb_height() - bar_h + (bar_h - FONT_HEIGHT) / 2,
                     buf, C_TEXT_DIM, 0, false);
}

static bool point_in(int32_t px, int32_t py,
                     int32_t x, int32_t y, int32_t w, int32_t h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

static bool icon_hovered(const icon& ic, int32_t mx, int32_t my) {
    return point_in(mx, my, ic.x, ic.y, ICON_W, ICON_H);
}

static void draw_icon(const icon& ic, bool hover) {
    int32_t x = ic.x;
    int32_t y = ic.y;

    // Drop shadow.
    fb_fill_rect(x + 3, y + 3, ICON_W, ICON_H, FB_RGB(0, 0, 0));

    switch (ic.app) {
        case APP_TERMINAL: {
            fb_fill_rect(x, y, ICON_W, ICON_H, C_TERM_BODY);
            fb_rect_outline(x, y, ICON_W, ICON_H, C_TERM_BORDER);
            font_draw_string_scaled(x + (ICON_W - 32) / 2,
                                    y + (ICON_H - 32) / 2,
                                    ">_", C_TERM_PROMPT, 2);
            break;
        }
        case APP_FILES: {
            fb_fill_rect(x, y + 12, ICON_W, ICON_H - 12, C_FILES_BODY);
            fb_fill_rect(x, y + 6,  36,     12,           C_FILES_BODY);
            fb_rect_outline(x, y + 6, ICON_W, ICON_H - 6, C_FILES_TAB);
            for (int i = 0; i < 3; i++)
                fb_fill_rect(x + 16, y + 30 + i * 12, ICON_W - 32, 4, C_FILES_TAB);
            break;
        }
        case APP_MONITOR: {
            fb_fill_rect(x, y, ICON_W, ICON_H, FB_RGB(0x0a, 0x18, 0x12));
            fb_rect_outline(x, y, ICON_W, ICON_H, C_MONITOR_BODY);
            // A tiny live "graph".
            uint64_t t = timer_get_ticks();
            int32_t bars = 14;
            int32_t bw = (ICON_W - 16) / bars;
            for (int i = 0; i < bars; i++) {
                uint32_t r = (uint32_t)((i * 37 + t) ^ 0xA5A5);
                int32_t bh = 6 + (int32_t)(r % 50);
                fb_fill_rect(x + 8 + i * bw,
                             y + ICON_H - 10 - bh,
                             bw - 1, bh, C_MONITOR_BODY);
            }
            break;
        }
        case APP_ABOUT: {
            fb_fill_rect(x, y, ICON_W, ICON_H, C_ABOUT_BODY);
            fb_rect_outline(x, y, ICON_W, ICON_H, C_ABOUT_HI);
            font_draw_string_scaled(x + (ICON_W - 16) / 2,
                                    y + (ICON_H - 32) / 2,
                                    "i", C_ABOUT_HI, 2);
            break;
        }
    }

    // Hover highlight.
    if (hover) {
        fb_rect_outline(x - 2, y - 2, ICON_W + 4, ICON_H + 4, C_ICON_HOVER);
        fb_rect_outline(x - 3, y - 3, ICON_W + 6, ICON_H + 6, C_ICON_HOVER);
    }

    // Label.
    uint32_t len = kstrlen(ic.label);
    int32_t lw = (int32_t)len * FONT_WIDTH;
    int32_t lx = x + (ICON_W - lw) / 2;
    int32_t ly = y + ICON_H + ICON_LABEL_GAP;
    font_draw_string(lx, ly, ic.label,
                     hover ? C_ICON_HOVER : C_ICON_LABEL, 0, false);
}

static void draw_icons() {
    const mouse_state* m = mouse_peek();
    for (int i = 0; i < N_ICONS; i++)
        draw_icon(icons[i], icon_hovered(icons[i], m->x, m->y));
}

static void draw_window_chrome(const window_t& w, bool focused) {
    // Drop shadow.
    for (int i = 1; i <= 4; i++) {
        fb_fill_rect(w.x + i, w.y + w.h + i - 1, w.w, 1, FB_RGB(0, 0, 0));
        fb_fill_rect(w.x + w.w + i - 1, w.y + i, 1, w.h, FB_RGB(0, 0, 0));
    }
    fb_color tcol = focused ? C_WIN_TITLE_F : C_WIN_TITLE;
    fb_fill_rect(w.x, w.y, w.w, TITLE_H, tcol);
    fb_fill_rect(w.x, w.y + TITLE_H, w.w, w.h - TITLE_H, C_WIN_BG);
    fb_rect_outline(w.x, w.y, w.w, w.h, C_WIN_BORDER);
    fb_fill_rect(w.x, w.y + TITLE_H, w.w, 1, C_WIN_BORDER);

    font_draw_string(w.x + 10, w.y + (TITLE_H - FONT_HEIGHT) / 2,
                     w.title, C_TEXT, 0, false);

    int32_t cx = w.x + w.w - CLOSE_INSET - CLOSE_W;
    int32_t cy = w.y + (TITLE_H - CLOSE_H) / 2;
    fb_fill_rect(cx, cy, CLOSE_W, CLOSE_H, FB_RGB(0xe6, 0x60, 0x60));
    fb_rect_outline(cx, cy, CLOSE_W, CLOSE_H, FB_RGB(0x60, 0x10, 0x10));
}

static bool point_in_close_btn(const window_t& w, int32_t px, int32_t py) {
    int32_t cx = w.x + w.w - CLOSE_INSET - CLOSE_W;
    int32_t cy = w.y + (TITLE_H - CLOSE_H) / 2;
    return point_in(px, py, cx, cy, CLOSE_W, CLOSE_H);
}

static bool point_in_titlebar(const window_t& w, int32_t px, int32_t py) {
    return point_in(px, py, w.x, w.y, w.w, TITLE_H);
}

// ----- App content renderers -----------------------------------------------
static void draw_terminal_in(const window_t& w) {
    int32_t bx = w.x + WIN_PAD;
    int32_t by = w.y + TITLE_H + WIN_PAD;
    int32_t avail_w = w.w - 2 * WIN_PAD;
    int32_t cols = avail_w / FONT_WIDTH;
    if (cols > TERM_COLS) cols = TERM_COLS;
    int32_t avail_h = w.h - TITLE_H - 2 * WIN_PAD;
    int32_t rows = avail_h / FONT_HEIGHT;
    if (rows > TERM_ROWS) rows = TERM_ROWS;

    for (int r = 0; r < rows; r++)
        font_draw_n(bx, by + r * FONT_HEIGHT, term.cells[r],
                    (uint32_t)cols, C_TERM_FG, 0, false);

    if (term.cur_row < rows && term.cur_col < cols) {
        int32_t cx = bx + term.cur_col * FONT_WIDTH;
        int32_t cy = by + term.cur_row * FONT_HEIGHT;
        // Blink at ~2 Hz.
        if (((timer_get_ticks() / 50) & 1) == 0)
            fb_fill_rect(cx, cy + FONT_HEIGHT - 2, FONT_WIDTH, 2, C_TERM_PROMPT);
    }
}

static void draw_about_in(const window_t& w) {
    int32_t bx = w.x + WIN_PAD;
    int32_t by = w.y + TITLE_H + WIN_PAD;

    font_draw_string_scaled(bx, by, "ParadoxOS", C_ABOUT_HI, 2);
    by += 36;
    font_draw_string(bx, by, "graphical kernel - tracks A-C", C_TEXT_DIM, 0, false);
    by += 24;

    font_draw_string(bx, by, "Architecture : x86_64 long mode", C_TEXT, 0, false); by += 18;
    font_draw_string(bx, by, "Bootloader   : Multiboot2 (GRUB)", C_TEXT, 0, false); by += 18;
    font_draw_string(bx, by, "Display      : 1024x768 32-bpp linear FB", C_TEXT, 0, false); by += 18;
    font_draw_string(bx, by, "Input        : PS/2 keyboard + mouse",   C_TEXT, 0, false); by += 18;
    font_draw_string(bx, by, "Memory mgmt  : PMM + paging + heap",     C_TEXT, 0, false); by += 18;
    font_draw_string(bx, by, "Storage      : ATA PIO (primary master)",C_TEXT, 0, false); by += 18;
    font_draw_string(bx, by, "Scheduler    : round-robin (kernel)",    C_TEXT, 0, false); by += 28;

    font_draw_string(bx, by, "Built with intent. No fake features.",   C_TERM_PROMPT, 0, false);
}

// System Monitor: live updating every redraw. We rely on gui_tick to
// repaint at least once a second (the clock tick) so the gauges move.
static void draw_monitor_in(const window_t& w) {
    int32_t bx = w.x + WIN_PAD;
    int32_t by = w.y + TITLE_H + WIN_PAD;
    int32_t bw = w.w - 2 * WIN_PAD;
    int32_t row_h = FONT_HEIGHT;

    char buf[96]; uint32_t n;

    // Uptime
    uint64_t ticks = timer_get_ticks();
    uint64_t s = ticks / 100;
    n = 0;
    n = cat(buf, sizeof(buf), n, "Uptime    : ");
    n += fmt_u64(buf + n, sizeof(buf) - n, s / 3600);
    n = cat(buf, sizeof(buf), n, "h ");
    n += fmt_u64(buf + n, sizeof(buf) - n, (s / 60) % 60);
    n = cat(buf, sizeof(buf), n, "m ");
    n += fmt_u64(buf + n, sizeof(buf) - n, s % 60);
    n = cat(buf, sizeof(buf), n, "s   ticks=");
    n += fmt_u64(buf + n, sizeof(buf) - n, ticks);
    font_draw_string(bx, by, buf, C_TEXT, 0, false);
    by += row_h + 8;

    // Memory bar
    uint64_t total_mb = pmm_get_total_memory() / (1024 * 1024);
    uint64_t used_mb  = pmm_get_used_memory()  / (1024 * 1024);
    uint64_t free_mb  = pmm_get_free_memory()  / (1024 * 1024);
    n = 0;
    n = cat(buf, sizeof(buf), n, "Memory    : ");
    n += fmt_u64(buf + n, sizeof(buf) - n, used_mb);
    n = cat(buf, sizeof(buf), n, " MiB used / ");
    n += fmt_u64(buf + n, sizeof(buf) - n, total_mb);
    n = cat(buf, sizeof(buf), n, " MiB total (");
    n += fmt_u64(buf + n, sizeof(buf) - n, free_mb);
    n = cat(buf, sizeof(buf), n, " free)");
    font_draw_string(bx, by, buf, C_TEXT, 0, false);
    by += row_h + 4;

    // Bar visual
    int32_t bar_w = bw - 16;
    int32_t bar_h = 16;
    int32_t fill_w = total_mb > 0 ? (int32_t)((uint64_t)bar_w * used_mb / total_mb) : 0;
    fb_rect_outline(bx, by, bar_w, bar_h, C_WIN_BORDER);
    fb_fill_rect(bx + 1, by + 1, fill_w, bar_h - 2, C_BAR_USED);
    fb_fill_rect(bx + 1 + fill_w, by + 1, bar_w - 2 - fill_w, bar_h - 2, C_BAR_FILL);
    by += bar_h + 12;

    // Devices count
    n = 0;
    n = cat(buf, sizeof(buf), n, "PCI devs  : ");
    n += fmt_u64(buf + n, sizeof(buf) - n, pci_get_device_count());
    font_draw_string(bx, by, buf, C_TEXT, 0, false);
    by += row_h;

    // Disk
    const ata_device* d = ata_primary_master();
    n = 0;
    n = cat(buf, sizeof(buf), n, "Disk      : ");
    if (d->present) {
        n = cat(buf, sizeof(buf), n, d->model);
        n = cat(buf, sizeof(buf), n, " (");
        n += fmt_u64(buf + n, sizeof(buf) - n, d->sectors / 2048);
        n = cat(buf, sizeof(buf), n, " MiB)");
    } else {
        n = cat(buf, sizeof(buf), n, "absent");
    }
    font_draw_string(bx, by, buf, C_TEXT, 0, false);
    by += row_h + 8;

    // Live "CPU activity" bar — really just pulses with the timer so you
    // see the whole panel updating in real time. Honest about what it is:
    n = 0;
    n = cat(buf, sizeof(buf), n, "Heartbeat : ");
    int32_t pulse_w = bw - 16;
    int32_t pulse_pos = (int32_t)((ticks % 100) * (pulse_w - 30) / 100);
    fb_fill_rect(bx, by + 6, pulse_w, 2, C_WIN_BORDER);
    fb_fill_rect(bx + pulse_pos, by + 4, 30, 6, C_TASKBAR_HI);
    font_draw_string(bx, by, buf, C_TEXT_DIM, 0, false);
    by += row_h + 8;

    font_draw_string(bx, by,
        "(updates every tick; close to free CPU)",
        C_TEXT_DIM, 0, false);
}

// Files / system browser. Surfaces real OS state until Track B/C bring
// a real filesystem.
static void draw_files_in(const window_t& w) {
    int32_t bx = w.x + WIN_PAD;
    int32_t by = w.y + TITLE_H + WIN_PAD;
    int32_t row_h = FONT_HEIGHT;

    char buf[160]; uint32_t n;

    font_draw_string(bx, by, "/system", C_TASKBAR_HI, 0, false);
    by += row_h + 4;

    // memory/
    font_draw_string(bx, by, "memory/", C_TEXT, 0, false); by += row_h;
    n = 0;
    n = cat(buf, sizeof(buf), n, "  total : ");
    n += fmt_u64(buf + n, sizeof(buf) - n, pmm_get_total_memory() / (1024 * 1024));
    n = cat(buf, sizeof(buf), n, " MiB,  used: ");
    n += fmt_u64(buf + n, sizeof(buf) - n, pmm_get_used_memory() / (1024 * 1024));
    n = cat(buf, sizeof(buf), n, " MiB,  free: ");
    n += fmt_u64(buf + n, sizeof(buf) - n, pmm_get_free_memory() / (1024 * 1024));
    n = cat(buf, sizeof(buf), n, " MiB");
    font_draw_string(bx, by, buf, C_TEXT_DIM, 0, false);
    by += row_h + 6;

    // disk/
    font_draw_string(bx, by, "disk/", C_TEXT, 0, false); by += row_h;
    const ata_device* d = ata_primary_master();
    n = 0;
    n = cat(buf, sizeof(buf), n, "  primary master : ");
    if (d->present) {
        n = cat(buf, sizeof(buf), n, d->model);
        n = cat(buf, sizeof(buf), n, "  ");
        n += fmt_u64(buf + n, sizeof(buf) - n, d->sectors);
        n = cat(buf, sizeof(buf), n, " sectors");
    } else {
        n = cat(buf, sizeof(buf), n, "(none)");
    }
    font_draw_string(bx, by, buf, C_TEXT_DIM, 0, false);
    by += row_h + 4;

    // Hex dump of the first 64 bytes of LBA 0.
    if (g_boot_sector_valid) {
        font_draw_string(bx, by, "  LBA 0 (first 64 bytes):", C_TEXT_DIM, 0, false);
        by += row_h;
        for (int line = 0; line < 4; line++) {
            n = 0;
            n = cat(buf, sizeof(buf), n, "    ");
            n += fmt_hex(buf + n, sizeof(buf) - n, line * 16, 4);
            n = cat(buf, sizeof(buf), n, "  ");
            for (int i = 0; i < 16; i++) {
                n += fmt_hex(buf + n, sizeof(buf) - n,
                             g_boot_sector[line * 16 + i], 2);
                buf[n++] = ' ';
                if (n + 1 >= sizeof(buf)) break;
            }
            buf[n] = 0;
            // Strip the "0x" prefixes that fmt_hex adds for compact display.
            // Easier: just write out a custom one. Quick post-process:
            char compact[160]; uint32_t cn = 0;
            for (uint32_t i = 0; i < n; i++) {
                if (buf[i] == '0' && i + 1 < n && buf[i + 1] == 'x') { i++; continue; }
                compact[cn++] = buf[i];
            }
            compact[cn] = 0;
            font_draw_string(bx, by, compact, C_TEXT_DIM, 0, false);
            by += row_h;
        }
    }
    by += 6;

    // devices/ (PCI)
    font_draw_string(bx, by, "devices/", C_TEXT, 0, false); by += row_h;
    uint32_t cnt = pci_get_device_count();
    for (uint32_t i = 0; i < cnt; i++) {
        pci_device* dev = pci_get_device(i);
        if (!dev) continue;
        n = 0;
        n = cat(buf, sizeof(buf), n, "  ");
        n += fmt_hex(buf + n, sizeof(buf) - n, dev->bus, 2);
        buf[n++] = ':';
        n += fmt_hex(buf + n, sizeof(buf) - n, dev->device, 2);
        buf[n++] = '.';
        n += fmt_u64(buf + n, sizeof(buf) - n, dev->function);
        buf[n++] = ' ';
        const uint32_t reserve_tail = 40;
        const char* cn2 = pci_get_class_name(dev->class_code);
        for (const char* s = cn2; *s && n + reserve_tail < sizeof(buf); s++) buf[n++] = *s;
        buf[n++] = ' ';
        buf[n++] = '(';
        const char* vn = pci_get_vendor_name(dev->vendor_id);
        for (const char* s = vn; *s && n + 2 < sizeof(buf); s++) buf[n++] = *s;
        if (n + 1 < sizeof(buf)) buf[n++] = ')';
        // Network device? show MMIO BAR for the curious.
        if (dev->class_code == PCI_CLASS_NETWORK) {
            n = cat(buf, sizeof(buf), n, "  BAR0=");
            n += fmt_hex(buf + n, sizeof(buf) - n, dev->bar[0], 8);
        }
        buf[n] = 0;
        font_draw_string(bx, by, buf, C_TEXT_DIM, 0, false);
        by += row_h;
        if (by + row_h > w.y + w.h - WIN_PAD) break;
    }
}

static void draw_window_content(const window_t& w) {
    switch (w.type) {
        case APP_TERMINAL: draw_terminal_in(w); break;
        case APP_FILES:    draw_files_in(w);    break;
        case APP_MONITOR:  draw_monitor_in(w);  break;
        case APP_ABOUT:    draw_about_in(w);    break;
    }
}

static void draw_cursor() {
    const mouse_state* m = mouse_peek();
    fb_blit_keyed(cursor_buf, CURSOR_W,
                  m->x, m->y, CURSOR_W, CURSOR_H, C_CURSOR_KEY);
}

static void redraw_all() {
    draw_desktop_bg();
    draw_icons();
    int focused = z_count > 0 ? z_order[z_count - 1] : -1;
    for (int i = 0; i < z_count; i++) {
        int idx = z_order[i];
        const window_t& w = windows[idx];
        draw_window_chrome(w, idx == focused);
        draw_window_content(w);
    }
    draw_cursor();
}

// ----- Click + drag dispatch ------------------------------------------------
static bool handle_left_press(int32_t cx, int32_t cy) {
    // Walk windows top-down.
    for (int i = z_count - 1; i >= 0; i--) {
        int idx = z_order[i];
        const window_t& w = windows[idx];
        if (!point_in(cx, cy, w.x, w.y, w.w, w.h)) continue;
        if (point_in_close_btn(w, cx, cy)) {
            close_window(idx);
            return true;
        }
        z_bring_to_top(idx);
        if (point_in_titlebar(w, cx, cy)) {
            // Begin drag.
            g_drag_idx = idx;
            g_drag_off_x = w.x - cx;
            g_drag_off_y = w.y - cy;
        }
        return true;
    }
    // Fall through to icons.
    for (int i = 0; i < N_ICONS; i++) {
        if (point_in(cx, cy, icons[i].x, icons[i].y, ICON_W, ICON_H)) {
            open_app(icons[i].app);
            return true;
        }
    }
    return false;
}

// ----- Splash ---------------------------------------------------------------
void gui_run_splash() {
    fb_clear(C_DESKTOP);

    const char* logo = "PARADOX OS";
    int scale = 6;
    uint32_t logo_chars = kstrlen(logo);
    int32_t total_w = (int32_t)logo_chars * FONT_WIDTH * scale;
    int32_t lx = ((int32_t)fb_width() - total_w) / 2;
    int32_t ly = (int32_t)fb_height() / 2 - 80;
    font_draw_string_scaled(lx, ly, logo, C_TASKBAR_HI, scale);

    const char* sub = "graphical kernel - tracks A-C";
    int32_t sub_w = (int32_t)kstrlen(sub) * FONT_WIDTH;
    int32_t sx = ((int32_t)fb_width() - sub_w) / 2;
    int32_t sy = ly + FONT_HEIGHT * scale + 24;
    font_draw_string(sx, sy, sub, C_TEXT_DIM, 0, false);

    int32_t bar_w = 480, bar_h = 8;
    int32_t bx = ((int32_t)fb_width() - bar_w) / 2;
    int32_t by = sy + 60;
    fb_rect_outline(bx, by, bar_w, bar_h, C_WIN_BORDER);

    uint64_t start = timer_get_ticks();
    uint64_t duration = 200;
    uint64_t now;
    do {
        now = timer_get_ticks();
        uint64_t dt = now - start;
        if (dt > duration) dt = duration;
        int32_t fill = (int32_t)((uint64_t)(bar_w - 2) * dt / duration);
        fb_fill_rect(bx + 1, by + 1, fill, bar_h - 2, C_TASKBAR_HI);
        fb_present();
        asm volatile("hlt");
    } while (now - start < duration);
}

// ----- Public API -----------------------------------------------------------
void gui_init() {
    cursor_compile();
    term_clear();

    for (int i = 0; i < MAX_WINDOWS; i++) windows[i].used = false;
    z_count = 0;

    // Snapshot LBA 0 once for the Files app to display.
    g_boot_sector_valid = ata_read(0, 1, g_boot_sector);
    if (g_boot_sector_valid) {
        serial_print("GUI: cached LBA 0 from primary master\n");
    }

    redraw_all();
    fb_present();
    serial_print("GUI: desktop initialized\n");
}

void gui_present() {
    redraw_all();
    fb_present();
}

void gui_term_putc(char c) {
    if (c == '\n')      term_newline();
    else if (c == '\r') term.cur_col = 0;
    else if (c == '\b') {
        if (term.cur_col > 0) {
            term.cur_col--;
            term.cells[term.cur_row][term.cur_col] = ' ';
        }
    } else if (c == '\t') {
        do { gui_term_putc(' '); } while (term.cur_col % 8 != 0);
    } else if (c >= 32 && c < 127) {
        if (term.cur_col >= TERM_COLS) term_newline();
        term.cells[term.cur_row][term.cur_col] = c;
        term.cur_col++;
    }
    term.dirty = true;
}

void gui_term_puts(const char* s) {
    for (uint32_t i = 0; s[i]; i++) gui_term_putc(s[i]);
}

void gui_tick() {
    mouse_event ev = mouse_consume_event();

    bool need = term.dirty;

    // --- Drag ---
    if (g_drag_idx >= 0) {
        // Update window position whenever the cursor has moved.
        if (windows[g_drag_idx].used) {
            int32_t nx = ev.x + g_drag_off_x;
            int32_t ny = ev.y + g_drag_off_y;
            // Keep at least the close-button row visible.
            int32_t W = (int32_t)fb_width();
            int32_t H = (int32_t)fb_height();
            if (nx < -windows[g_drag_idx].w + 80) nx = -windows[g_drag_idx].w + 80;
            if (nx > W - 80)            nx = W - 80;
            if (ny < 0)                 ny = 0;
            if (ny > H - TITLE_H - 32)  ny = H - TITLE_H - 32;
            if (windows[g_drag_idx].x != nx || windows[g_drag_idx].y != ny) {
                windows[g_drag_idx].x = nx;
                windows[g_drag_idx].y = ny;
                need = true;
            }
        }
        if (ev.released & MOUSE_BUTTON_LEFT) g_drag_idx = -1;
    } else if (ev.pressed & MOUSE_BUTTON_LEFT) {
        if (handle_left_press(ev.x, ev.y)) need = true;
    }

    // Cursor motion always redraws so the arrow follows.
    static int32_t last_x = -1, last_y = -1;
    if (ev.x != last_x || ev.y != last_y) {
        last_x = ev.x; last_y = ev.y;
        need = true;
    }

    // Time-based redraw (clock, monitor, starfield motion).
    static uint64_t last_paint_tick = 0;
    uint64_t now = timer_get_ticks();
    // Repaint at ~10 Hz so the starfield + monitor look alive.
    if (now - last_paint_tick >= 10) {
        last_paint_tick = now;
        need = true;
    }

    if (!need) return;
    term.dirty = false;
    gui_present();
}
