// Desktop compositor: splash + starfield bg + icon column + window
// manager + taskbar. Repaint is event-driven from gui_tick.

#include "../include/gui.h"
#include "../include/framebuffer.h"
#include "../include/font.h"
#include "../include/mouse.h"
#include "../include/serial.h"
#include "../include/timer.h"
#include "../include/memory.h"
#include "../include/pci.h"
#include "../include/ata.h"
#include "../include/process.h"
#include "../include/vfs.h"
#include "../include/diskfs.h"

static const fb_color C_DESKTOP    = FB_RGB(0x07, 0x04, 0x1c);  // splash bg
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

#define TERM_COLS 96
#define TERM_ROWS 36
struct term_state {
    char     cells[TERM_ROWS][TERM_COLS];
    int32_t  cur_row;
    int32_t  cur_col;
    bool     dirty;
};
static term_state term;

enum app_type {
    APP_TERMINAL    = 0,
    APP_FILES       = 1,
    APP_MONITOR     = 2,
    APP_ABOUT       = 3,
    APP_EDITOR      = 4,
    APP_CALCULATOR  = 5,
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

static int     g_drag_idx = -1;
static int32_t g_drag_off_x = 0;
static int32_t g_drag_off_y = 0;

struct icon {
    int32_t  x, y;
    const char* label;
    app_type app;
};
#define ICON_W 80
#define ICON_H 80
#define ICON_LABEL_GAP 4

#define N_ICONS 5
static icon icons[N_ICONS] = {
    { 30,  40,  "Terminal",   APP_TERMINAL    },
    { 30,  150, "Files",      APP_FILES       },
    { 30,  260, "Monitor",    APP_MONITOR     },
    { 30,  370, "Calculator", APP_CALCULATOR  },
    { 30,  480, "About",      APP_ABOUT       },
};

// Read once at gui_init so Files can show real LBA-0 bytes without PIO per frame.
static uint8_t g_boot_sector[512];
static bool    g_boot_sector_valid = false;

static vfs_node* g_files_dir      = nullptr;
static vfs_node* g_files_selected = nullptr;
static vfs_node* g_editor_file    = nullptr;

static uint64_t g_last_save_tick = 0;

// Two-operand desk calc. op is 0 / '+' / '-' / '*' / '/'.
static char     g_calc_display[24] = "0";
static int64_t  g_calc_acc = 0;
static char     g_calc_op  = 0;
static bool     g_calc_fresh = true;

// Hit rects computed on draw, read on click.
struct calc_btn { int32_t x, y, w, h; char label[4]; };
static calc_btn g_calc_btns[20];
static int      g_calc_btn_count = 0;

struct btn_rect { int32_t x, y, w, h; };
static btn_rect g_btn_open  = { 0, 0, 0, 0 };
static btn_rect g_btn_edit  = { 0, 0, 0, 0 };
static btn_rect g_btn_run   = { 0, 0, 0, 0 };
static btn_rect g_btn_save  = { 0, 0, 0, 0 };

#define FILES_MAX_ROWS 64
struct files_row { int32_t y; vfs_node* node; };
static files_row g_files_rows[FILES_MAX_ROWS];
static int       g_files_row_count = 0;
static int32_t   g_files_row_x = 0;
static int32_t   g_files_row_w = 0;
static int32_t   g_files_row_h = FONT_HEIGHT + 4;

#define FILES_ARGS_CAP 96
static char      g_args_buf[FILES_ARGS_CAP] = { 0 };
static uint32_t  g_args_len      = 0;
static bool      g_args_focused  = false;
static btn_rect  g_args_field    = { 0, 0, 0, 0 };

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
        case APP_TERMINAL:   return "Terminal";
        case APP_FILES:      return "Files";
        case APP_MONITOR:    return "System Monitor";
        case APP_ABOUT:      return "About ParadoxOS";
        case APP_EDITOR:     return "Editor";
        case APP_CALCULATOR: return "Calculator";
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
        case APP_FILES:      w = 760; h = 520; break;
        case APP_MONITOR:    w = 540; h = 380; break;
        case APP_ABOUT:      w = 500; h = 340; break;
        case APP_EDITOR:     w = 720; h = 520; break;
        case APP_CALCULATOR: w = 280; h = 360; break;
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

#define TASKBAR_H            32
#define TASKBAR_BTN_W        140
#define TASKBAR_BTN_H        24
#define TASKBAR_BTN_GAP      6
#define TASKBAR_BTN_LEFT     110   // brand sits to the left of this

static bool taskbar_btn_rect(int z_idx,
                             int32_t* x, int32_t* y,
                             int32_t* w, int32_t* h) {
    if (z_idx < 0 || z_idx >= z_count) return false;
    int32_t base_x = TASKBAR_BTN_LEFT + z_idx * (TASKBAR_BTN_W + TASKBAR_BTN_GAP);
    int32_t base_y = (int32_t)fb_height() - TASKBAR_H + (TASKBAR_H - TASKBAR_BTN_H) / 2;
    *x = base_x; *y = base_y; *w = TASKBAR_BTN_W; *h = TASKBAR_BTN_H;
    return true;
}

static fb_color lerp_rgb(fb_color a, fb_color b, int32_t t, int32_t max) {
    if (max <= 0) return a;
    if (t < 0) t = 0;
    if (t > max) t = max;
    int32_t ar = (int32_t)((a >> 16) & 0xFF), ag = (int32_t)((a >> 8) & 0xFF), ab = (int32_t)(a & 0xFF);
    int32_t br = (int32_t)((b >> 16) & 0xFF), bg = (int32_t)((b >> 8) & 0xFF), bb = (int32_t)(b & 0xFF);
    int32_t r = ar + ((br - ar) * t) / max;
    int32_t g = ag + ((bg - ag) * t) / max;
    int32_t bc = ab + ((bb - ab) * t) / max;
    return FB_RGB((uint8_t)r, (uint8_t)g, (uint8_t)bc);
}

static void draw_stars_above(int32_t max_y) {
    uint64_t t = timer_get_ticks();
    const int N_STARS = 90;
    for (int i = 0; i < N_STARS; i++) {
        uint32_t px = 0xC0DEF00D ^ ((uint32_t)i * 0x9E3779B9u);
        uint32_t py = 0xDEADBEEF ^ ((uint32_t)i * 0x85EBCA6Bu);
        uint32_t speed = 1 + (px % 4);
        int32_t x = (int32_t)((px ^ (uint32_t)((t * speed) >> 4)) % fb_width());
        int32_t y = (int32_t)(py % (uint32_t)max_y);
        uint8_t bri = 90 + (uint8_t)(px % 160);
        fb_put_pixel(x, y, FB_RGB(bri, bri, (uint8_t)(bri | 40)));
        if (bri > 210) {
            fb_put_pixel(x - 1, y, FB_RGB(bri / 2, bri / 2, bri / 2));
            fb_put_pixel(x - 2, y, FB_RGB(bri / 4, bri / 4, bri / 4));
        }
    }
}

static int32_t isqrt32(int32_t v) {
    if (v <= 0) return 0;
    int32_t r = 0;
    while ((r + 1) * (r + 1) <= v) r++;
    return r;
}

// Synthwave wallpaper.
static void draw_desktop_bg() {
    int32_t W = (int32_t)fb_width();
    int32_t H = (int32_t)fb_height();
    int32_t taskbar_top = H - TASKBAR_H;
    int32_t horizon_y   = (H * 58) / 100;

    const fb_color sky_top     = FB_RGB(0x07, 0x04, 0x1c);
    const fb_color sky_mid     = FB_RGB(0x36, 0x0e, 0x56);
    const fb_color sky_horizon = FB_RGB(0xe0, 0x46, 0x9a);
    const fb_color ground_near = FB_RGB(0x06, 0x02, 0x16);
    const fb_color ground_far  = FB_RGB(0x22, 0x06, 0x36);
    const fb_color grid_near   = FB_RGB(0xff, 0x4a, 0xc8);
    const fb_color grid_far    = FB_RGB(0x40, 0x08, 0x44);

    // ---- Sky: two-stop vertical gradient (sky_top -> sky_mid -> sky_horizon)
    int32_t mid_y = horizon_y / 2;
    for (int32_t y = 0; y < horizon_y; y++) {
        fb_color c = (y < mid_y)
            ? lerp_rgb(sky_top, sky_mid, y, mid_y)
            : lerp_rgb(sky_mid, sky_horizon, y - mid_y, horizon_y - mid_y);
        fb_fill_rect(0, y, W, 1, c);
    }
    draw_stars_above(horizon_y * 7 / 10);

    // Sun (banded half-disc on the horizon).
    int32_t sun_cx = W / 2;
    int32_t sun_cy = horizon_y;
    int32_t sun_r  = 130;
    const fb_color sun_top = FB_RGB(0xff, 0xe0, 0x6a);
    const fb_color sun_bot = FB_RGB(0xff, 0x46, 0x9a);
    for (int32_t y = sun_cy - sun_r; y < sun_cy; y++) {
        int32_t dy = sun_cy - y;
        int32_t hw = isqrt32(sun_r * sun_r - dy * dy);
        int32_t band_idx = (sun_r - dy) / 10;
        if (dy < sun_r * 2 / 3 && (band_idx & 1) == 0) continue;
        fb_color c = lerp_rgb(sun_top, sun_bot, sun_r - dy, sun_r);
        fb_fill_rect(sun_cx - hw, y, hw * 2, 1, c);
    }

    fb_fill_rect(0, horizon_y - 1, W, 1, FB_RGB(0xff, 0xb0, 0xd8));
    fb_fill_rect(0, horizon_y,     W, 1, FB_RGB(0xff, 0x60, 0xb0));

    int32_t ground_h = taskbar_top - horizon_y;
    for (int32_t y = horizon_y + 1; y < taskbar_top; y++) {
        fb_color c = lerp_rgb(ground_far, ground_near, y - horizon_y, ground_h);
        fb_fill_rect(0, y, W, 1, c);
    }

    // Perspective grid: horizontal spacing grows toward the viewer,
    // verticals converge on (sun_cx, horizon_y).
    {
        int32_t y_off = 4;
        int32_t step  = 4;
        while (horizon_y + y_off < taskbar_top) {
            int32_t y = horizon_y + y_off;
            int32_t t = (y - horizon_y) * 255 / ground_h;
            fb_color c = lerp_rgb(grid_far, grid_near, t, 255);
            fb_fill_rect(0, y, W, 1, c);
            y_off += step;
            step  += 2;
        }
    }
    {
        const int N_LINES = 19;
        for (int i = 0; i < N_LINES; i++) {
            int32_t x_bottom = (int32_t)((int64_t)i * (int64_t)W / (int64_t)(N_LINES - 1));
            for (int32_t y = horizon_y + 1; y < taskbar_top; y++) {
                int32_t x = sun_cx + ((x_bottom - sun_cx) * (y - horizon_y)) / ground_h;
                int32_t t = (y - horizon_y) * 255 / ground_h;
                fb_color c = lerp_rgb(grid_far, grid_near, t, 255);
                fb_put_pixel(x, y, c);
            }
        }
    }

    int32_t bar_h = TASKBAR_H;
    fb_fill_rect(0, fb_height() - bar_h, fb_width(), bar_h, C_TASKBAR);
    fb_fill_rect(0, fb_height() - bar_h, fb_width(), 1, C_TASKBAR_HI);
    font_draw_string(10, fb_height() - bar_h + (bar_h - FONT_HEIGHT) / 2,
                     "ParadoxOS", C_TASKBAR_HI, 0, false);

    int focused = z_count > 0 ? z_order[z_count - 1] : -1;
    for (int i = 0; i < z_count; i++) {
        int32_t bx, by, bw, bh;
        if (!taskbar_btn_rect(i, &bx, &by, &bw, &bh)) break;
        int idx = z_order[i];
        bool is_focused = (idx == focused);
        fb_color bg = is_focused ? C_WIN_TITLE_F : C_WIN_TITLE;
        fb_fill_rect(bx, by, bw, bh, bg);
        fb_rect_outline(bx, by, bw, bh, C_WIN_BORDER);
        const char* t = windows[idx].title;
        int max_chars = (bw - 12) / FONT_WIDTH;
        font_draw_n(bx + 6, by + (bh - FONT_HEIGHT) / 2, t,
                    (uint32_t)max_chars, C_TEXT, 0, false);
    }

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
        case APP_EDITOR:
            // No desktop icon for the editor — it's launched from Files.
            break;
        case APP_CALCULATOR: {
            // Olive-green calculator with a small "+" embossed inside.
            fb_fill_rect(x, y, ICON_W, ICON_H, FB_RGB(0x55, 0x77, 0x88));
            fb_rect_outline(x, y, ICON_W, ICON_H, FB_RGB(0xff, 0xff, 0xff));
            // Mini display
            fb_fill_rect(x + 8, y + 8, ICON_W - 16, 16, FB_RGB(0x05, 0x10, 0x18));
            // Button grid hint
            for (int r = 0; r < 3; r++)
                for (int c = 0; c < 3; c++)
                    fb_fill_rect(x + 10 + c * 21, y + 30 + r * 15, 16, 10,
                                 FB_RGB(0x33, 0x55, 0x66));
            font_draw_string_scaled(x + (ICON_W - 16) / 2,
                                    y + (ICON_H - 32) / 2 + 6,
                                    "+", C_TEXT, 2);
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
    char dbuf[64]; uint32_t dn = 0;
    dn = cat(dbuf, sizeof(dbuf), dn, "Display      : ");
    dn += fmt_u64(dbuf + dn, sizeof(dbuf) - dn, fb_width());
    dn = cat(dbuf, sizeof(dbuf), dn, "x");
    dn += fmt_u64(dbuf + dn, sizeof(dbuf) - dn, fb_height());
    dn = cat(dbuf, sizeof(dbuf), dn, " 32-bpp linear FB");
    font_draw_string(bx, by, dbuf, C_TEXT, 0, false); by += 18;
    font_draw_string(bx, by, "Input        : PS/2 keyboard + mouse",   C_TEXT, 0, false); by += 18;
    font_draw_string(bx, by, "Memory mgmt  : PMM + paging + heap",     C_TEXT, 0, false); by += 18;
    font_draw_string(bx, by, "Storage      : ATA PIO + diskfs (disk-backed /home)", C_TEXT, 0, false); by += 18;
    font_draw_string(bx, by, "User mode    : ring 3 + ELF64 loader",   C_TEXT, 0, false); by += 18;
    font_draw_string(bx, by, "Scheduler    : round-robin (kernel)",    C_TEXT, 0, false); by += 28;

    font_draw_string(bx, by, "Built with intent. No fake features.",   C_TERM_PROMPT, 0, false);
}

// Polled live by gui_tick's periodic repaint.
static void draw_monitor_in(const window_t& w) {
    int32_t bx = w.x + WIN_PAD;
    int32_t by = w.y + TITLE_H + WIN_PAD;
    int32_t bw = w.w - 2 * WIN_PAD;
    int32_t row_h = FONT_HEIGHT;

    char buf[96]; uint32_t n;

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
    uint64_t total_mb = pmm_get_managed_memory() / (1024 * 1024);
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

    int32_t bar_w = bw - 16;
    int32_t bar_h = 16;
    int32_t fill_w = total_mb > 0 ? (int32_t)((uint64_t)bar_w * used_mb / total_mb) : 0;
    fb_rect_outline(bx, by, bar_w, bar_h, C_WIN_BORDER);
    fb_fill_rect(bx + 1, by + 1, fill_w, bar_h - 2, C_BAR_USED);
    fb_fill_rect(bx + 1 + fill_w, by + 1, bar_w - 2 - fill_w, bar_h - 2, C_BAR_FILL);
    by += bar_h + 12;

    n = 0;
    n = cat(buf, sizeof(buf), n, "PCI devs  : ");
    n += fmt_u64(buf + n, sizeof(buf) - n, pci_get_device_count());
    font_draw_string(bx, by, buf, C_TEXT, 0, false);
    by += row_h;

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

    // Pulses with the timer so you can see the panel updating.
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

//
static const char* kind_label(vfs_kind k) {
    switch (k) {
        case VFS_DIR:  return "dir";
        case VFS_FILE: return "file";
        case VFS_EXEC: return "exec";
    }
    return "?";
}

static void draw_filesys_button(btn_rect r, const char* label,
                                bool enabled, bool primary) {
    fb_color fill = primary && enabled ? C_TASKBAR_HI : C_WIN_TITLE_F;
    fb_color text = enabled ? C_TEXT : C_TEXT_DIM;
    if (!enabled) fill = C_WIN_TITLE;
    fb_fill_rect(r.x, r.y, r.w, r.h, fill);
    fb_rect_outline(r.x, r.y, r.w, r.h, C_WIN_BORDER);
    uint32_t len = kstrlen(label);
    int32_t lw = (int32_t)len * FONT_WIDTH;
    int32_t lx = r.x + (r.w - lw) / 2;
    int32_t ly = r.y + (r.h - FONT_HEIGHT) / 2;
    font_draw_string(lx, ly, label, text, 0, false);
}

static void draw_files_in(const window_t& w) {
    if (!g_files_dir) g_files_dir = vfs_root();

    int32_t bx = w.x + WIN_PAD;
    int32_t by = w.y + TITLE_H + WIN_PAD;
    int32_t bw = w.w - 2 * WIN_PAD;
    int32_t bh = w.h - TITLE_H - 2 * WIN_PAD;

    int32_t btn_h = 22;
    int32_t btn_w = 60;
    int32_t btn_gap = 6;
    int32_t toolbar_y = by;

    bool has_sel  = (g_files_selected != nullptr);
    bool sel_file = has_sel && g_files_selected->kind == VFS_FILE;
    bool sel_exec = has_sel && g_files_selected->kind == VFS_EXEC;

    g_btn_open = {bx,                       toolbar_y, btn_w, btn_h};
    g_btn_edit = {bx + (btn_w + btn_gap),   toolbar_y, btn_w, btn_h};
    g_btn_run  = {bx + 2 * (btn_w + btn_gap), toolbar_y, btn_w, btn_h};

    draw_filesys_button(g_btn_open, "Open", has_sel, true);
    draw_filesys_button(g_btn_edit, "Edit", sel_file, false);
    draw_filesys_button(g_btn_run,  "Run",  sel_exec, false);

    int32_t path_x = g_btn_run.x + g_btn_run.w + 16;
    char pbuf[128];
    vfs_path_of(g_files_dir, pbuf, sizeof(pbuf));
    font_draw_string(path_x, toolbar_y + (btn_h - FONT_HEIGHT) / 2, pbuf,
                     C_TASKBAR_HI, 0, false);

    // Args input — typed text feeds vfs_run as argv when Run is clicked.
    int32_t args_y = toolbar_y + btn_h + 6;
    font_draw_string(bx, args_y + 4, "args:", C_TEXT_DIM, 0, false);
    g_args_field = { bx + 48, args_y, bw - 48, btn_h };
    fb_color border = g_args_focused ? C_TASKBAR_HI : C_WIN_BORDER;
    fb_fill_rect(g_args_field.x, g_args_field.y,
                 g_args_field.w, g_args_field.h, C_WIN_BG);
    fb_rect_outline(g_args_field.x, g_args_field.y,
                    g_args_field.w, g_args_field.h, border);
    font_draw_string(g_args_field.x + 6, g_args_field.y + 4,
                     g_args_buf, C_TEXT, 0, false);
    if (g_args_focused) {
        int32_t caret_x = g_args_field.x + 6 + (int32_t)g_args_len * FONT_WIDTH;
        fb_fill_rect(caret_x, g_args_field.y + 4, 2, FONT_HEIGHT, C_TEXT);
    }

    int32_t list_y = args_y + btn_h + 10;

    fb_fill_rect(bx, list_y, bw, FONT_HEIGHT + 4, C_WIN_TITLE);
    font_draw_string(bx + 8, list_y + 2, "name",  C_TEXT_DIM, 0, false);
    font_draw_string(bx + 280, list_y + 2, "kind", C_TEXT_DIM, 0, false);
    font_draw_string(bx + 360, list_y + 2, "size", C_TEXT_DIM, 0, false);
    font_draw_string(bx + 460, list_y + 2, "description", C_TEXT_DIM, 0, false);

    // Rows
    int32_t rows_y0 = list_y + FONT_HEIGHT + 6;
    int32_t row_h   = g_files_row_h;
    int32_t cur_y   = rows_y0;
    g_files_row_count = 0;
    g_files_row_x = bx;
    g_files_row_w = bw;

    auto draw_row = [&](vfs_node* n, const char* name_override) {
        if (g_files_row_count >= FILES_MAX_ROWS) return;
        if (cur_y + row_h > by + bh) return;

        bool selected = (g_files_selected == n);
        if (selected) fb_fill_rect(bx, cur_y, bw, row_h, C_WIN_TITLE_F);

        const char* name = name_override ? name_override : n->name;
        char prefix[3] = " ";
        if (n) {
            switch (n->kind) {
                case VFS_DIR:  prefix[0] = '['; prefix[1] = ']'; prefix[2] = 0; break;
                case VFS_FILE: prefix[0] = '-'; prefix[1] = 0;  break;
                case VFS_EXEC: prefix[0] = '*'; prefix[1] = 0;  break;
            }
        }
        char namebuf[64]; uint32_t nn = 0;
        for (uint32_t i = 0; prefix[i] && nn + 1 < sizeof(namebuf); i++)
            namebuf[nn++] = prefix[i];
        if (prefix[0]) namebuf[nn++] = ' ';
        for (uint32_t i = 0; name[i] && nn + 1 < sizeof(namebuf); i++)
            namebuf[nn++] = name[i];
        namebuf[nn] = 0;
        font_draw_string(bx + 8, cur_y + 2, namebuf, C_TEXT, 0, false);

        if (n) {
            font_draw_string(bx + 280, cur_y + 2,
                             kind_label(n->kind), C_TEXT_DIM, 0, false);
            if (n->kind == VFS_FILE) {
                char sb[16]; uint32_t sn = fmt_u64(sb, sizeof(sb), n->len);
                (void)sn;
                font_draw_string(bx + 360, cur_y + 2, sb, C_TEXT_DIM, 0, false);
            }
            if (n->kind == VFS_EXEC && n->description) {
                // Clip — overflow text outside the window doesn't get
                // cleared on redraw and ends up smeared on the desktop.
                int32_t desc_x   = bx + 460;
                int32_t desc_max = (bx + bw) - desc_x - 4;
                uint32_t max_chars = (desc_max > 0)
                                     ? (uint32_t)(desc_max / FONT_WIDTH) : 0;
                char dbuf[64];
                uint32_t di = 0;
                for (; di < max_chars && di + 1 < sizeof(dbuf)
                       && n->description[di]; di++) {
                    dbuf[di] = n->description[di];
                }
                if (n->description[di] && di >= 3) {
                    dbuf[di - 3] = '.';
                    dbuf[di - 2] = '.';
                    dbuf[di - 1] = '.';
                }
                dbuf[di] = 0;
                font_draw_string(desc_x, cur_y + 2, dbuf,
                                 C_TEXT_DIM, 0, false);
            }
        }

        g_files_rows[g_files_row_count++] = { cur_y, n };
        cur_y += row_h;
    };

    if (g_files_dir->parent) draw_row(g_files_dir->parent, "..");
    for (vfs_node* c = g_files_dir->first_child; c; c = c->next) {
        draw_row(c, nullptr);
    }
}

// Append-only editor. Edits land directly in the VFS file content;
// Save flushes to disk via diskfs.
static void draw_editor_in(const window_t& w) {
    int32_t bx = w.x + WIN_PAD;
    int32_t by = w.y + TITLE_H + WIN_PAD;
    int32_t bw = w.w - 2 * WIN_PAD;
    int32_t bh = w.h - TITLE_H - 2 * WIN_PAD;

    int32_t btn_h = 22;
    int32_t btn_w = 60;
    g_btn_save = {bx, by, btn_w, btn_h};
    draw_filesys_button(g_btn_save, "Save", true, true);

    char pbuf[128];
    vfs_path_of(g_editor_file, pbuf, sizeof(pbuf));
    int32_t label_y = by + (btn_h - FONT_HEIGHT) / 2;
    font_draw_string(bx + btn_w + 16, label_y, pbuf, C_TASKBAR_HI, 0, false);

    // Toast for ~2s after Save.
    if (g_last_save_tick && timer_get_ticks() - g_last_save_tick < 200) {
        int32_t toast_x = bx + btn_w + 16 + (int32_t)kstrlen(pbuf) * FONT_WIDTH + 16;
        font_draw_string(toast_x, label_y,
                         "[saved to disk]", C_TERM_PROMPT, 0, false);
    }

    int32_t body_y = by + btn_h + 10;
    int32_t body_h = bh - (btn_h + 10);
    fb_rect_outline(bx, body_y, bw, body_h, C_WIN_BORDER);

    if (!g_editor_file) return;

    // Render content: 8-pixel line height, monospace. Wrap on newlines
    // and on the right edge of the content area.
    int32_t pad = 6;
    int32_t cx = bx + pad;
    int32_t cy = body_y + pad;
    int32_t row_w = bw - 2 * pad;
    int32_t cols  = row_w / FONT_WIDTH;

    int32_t col = 0;
    for (uint32_t i = 0; i < g_editor_file->len; i++) {
        char c = g_editor_file->content[i];
        if (c == '\n') {
            cy += FONT_HEIGHT;
            col = 0;
            if (cy + FONT_HEIGHT > body_y + body_h) break;
            continue;
        }
        if (col >= cols) {
            cy += FONT_HEIGHT;
            col = 0;
            if (cy + FONT_HEIGHT > body_y + body_h) break;
        }
        font_draw_char(cx + col * FONT_WIDTH, cy, c,
                       C_TERM_FG, 0, false);
        col++;
    }

    if (((timer_get_ticks() / 50) & 1) == 0) {
        int32_t caret_x = cx + col * FONT_WIDTH;
        int32_t caret_y = cy;
        fb_fill_rect(caret_x, caret_y + FONT_HEIGHT - 2,
                     FONT_WIDTH, 2, C_TERM_PROMPT);
    }
}

static char g_editor_title[96];

static void open_editor_for(vfs_node* file) {
    if (!file || file->kind != VFS_FILE) return;
    g_editor_file = file;

    char path[80];
    vfs_path_of(file, path, sizeof(path));
    uint32_t n = 0;
    static const char* prefix = "Editor - ";
    for (uint32_t i = 0; prefix[i] && n + 1 < sizeof(g_editor_title); i++)
        g_editor_title[n++] = prefix[i];
    for (uint32_t i = 0; path[i] && n + 1 < sizeof(g_editor_title); i++)
        g_editor_title[n++] = path[i];
    g_editor_title[n] = 0;

    int existing = find_window_of_type(APP_EDITOR);
    if (existing >= 0) {
        windows[existing].title = g_editor_title;
        z_bring_to_top(existing);
        term.dirty = true;
        return;
    }
    open_app(APP_EDITOR);
    int idx = find_window_of_type(APP_EDITOR);
    if (idx >= 0) windows[idx].title = g_editor_title;
}


static void calc_set_display_from_int(int64_t v) {
    bool neg = v < 0; if (neg) v = -v;
    char tmp[24]; int ti = 0;
    if (v == 0) tmp[ti++] = '0';
    while (v) { tmp[ti++] = '0' + (int)(v % 10); v /= 10; }
    int oi = 0;
    if (neg) g_calc_display[oi++] = '-';
    while (ti > 0 && oi + 1 < (int)sizeof(g_calc_display))
        g_calc_display[oi++] = tmp[--ti];
    g_calc_display[oi] = 0;
}

static int64_t calc_parse_display() {
    const char* s = g_calc_display;
    bool neg = false;
    if (*s == '-') { neg = true; s++; }
    int64_t v = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
    return neg ? -v : v;
}

static void calc_press(const char* label) {
    if (label[0] >= '0' && label[0] <= '9' && label[1] == 0) {
        if (g_calc_fresh) {
            g_calc_display[0] = label[0];
            g_calc_display[1] = 0;
            g_calc_fresh = false;
        } else {
            uint32_t n = kstrlen(g_calc_display);
            if (n + 1 < sizeof(g_calc_display) - 1) {
                if (n == 1 && g_calc_display[0] == '0') {
                    g_calc_display[0] = label[0];
                } else {
                    g_calc_display[n] = label[0];
                    g_calc_display[n + 1] = 0;
                }
            }
        }
        return;
    }
    if (label[0] == '+' || label[0] == '-' ||
        label[0] == '*' || label[0] == '/') {
        if (g_calc_op && !g_calc_fresh) {
            int64_t b = calc_parse_display();
            int64_t r = g_calc_acc;
            switch (g_calc_op) {
                case '+': r = g_calc_acc + b; break;
                case '-': r = g_calc_acc - b; break;
                case '*': r = g_calc_acc * b; break;
                case '/': r = b ? g_calc_acc / b : 0; break;
            }
            g_calc_acc = r;
            calc_set_display_from_int(r);
        } else {
            g_calc_acc = calc_parse_display();
        }
        g_calc_op = label[0];
        g_calc_fresh = true;
        return;
    }
    if (label[0] == '=') {
        if (g_calc_op) {
            int64_t b = calc_parse_display();
            int64_t r = g_calc_acc;
            switch (g_calc_op) {
                case '+': r = g_calc_acc + b; break;
                case '-': r = g_calc_acc - b; break;
                case '*': r = g_calc_acc * b; break;
                case '/': r = b ? g_calc_acc / b : 0; break;
            }
            calc_set_display_from_int(r);
            g_calc_acc = r;
            g_calc_op = 0;
            g_calc_fresh = true;
        }
        return;
    }
    if (label[0] == 'C') {
        g_calc_display[0] = '0';
        g_calc_display[1] = 0;
        g_calc_acc = 0;
        g_calc_op = 0;
        g_calc_fresh = true;
        return;
    }
}

static const char* k_calc_labels[16] = {
    "C", "(", ")", "/",
    "7", "8", "9", "*",
    "4", "5", "6", "-",
    "1", "2", "3", "+",
};

static void draw_calculator_in(const window_t& w) {
    int32_t bx = w.x + WIN_PAD;
    int32_t by = w.y + TITLE_H + WIN_PAD;
    int32_t bw = w.w - 2 * WIN_PAD;

    int32_t disp_h = 40;
    fb_fill_rect(bx, by, bw, disp_h, FB_RGB(0x05, 0x10, 0x18));
    fb_rect_outline(bx, by, bw, disp_h, C_WIN_BORDER);
    uint32_t dlen = kstrlen(g_calc_display);
    int32_t dlen_px = (int32_t)dlen * FONT_WIDTH * 2;
    int32_t dx = bx + bw - 8 - dlen_px;
    int32_t dy = by + (disp_h - FONT_HEIGHT * 2) / 2;
    font_draw_string_scaled(dx, dy, g_calc_display, C_TERM_PROMPT, 2);

    int32_t status_y = by + disp_h + 4;
    char status[12]; uint32_t sn = 0;
    static const char* prefix = "op: ";
    while (prefix[sn]) { status[sn] = prefix[sn]; sn++; }
    status[sn++] = g_calc_op ? g_calc_op : '-';
    status[sn] = 0;
    font_draw_string(bx + 4, status_y, status, C_TEXT_DIM, 0, false);

    int32_t grid_y = by + disp_h + 24;
    int32_t cell_pad = 4;
    int32_t cell_w = (bw - cell_pad * 3) / 4;
    int32_t cell_h = 44;

    g_calc_btn_count = 0;
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            const char* lbl = k_calc_labels[row * 4 + col];
            int32_t cx = bx + col * (cell_w + cell_pad);
            int32_t cy = grid_y + row * (cell_h + cell_pad);
            bool is_op = !(lbl[0] >= '0' && lbl[0] <= '9');
            fb_color bg = is_op ? FB_RGB(0x3a, 0x6a, 0x9a) : FB_RGB(0x2a, 0x34, 0x4a);
            fb_fill_rect(cx, cy, cell_w, cell_h, bg);
            fb_rect_outline(cx, cy, cell_w, cell_h, C_WIN_BORDER);
            int32_t tx = cx + (cell_w - FONT_WIDTH * 2) / 2;
            int32_t ty = cy + (cell_h - FONT_HEIGHT * 2) / 2;
            font_draw_string_scaled(tx, ty, lbl, C_TEXT, 2);

            calc_btn& b = g_calc_btns[g_calc_btn_count++];
            b.x = cx; b.y = cy; b.w = cell_w; b.h = cell_h;
            b.label[0] = lbl[0]; b.label[1] = 0;
        }
    }
    int32_t row_y = grid_y + 4 * (cell_h + cell_pad);
    int32_t wide  = cell_w * 2 + cell_pad;
    {
        int32_t cx = bx;
        fb_fill_rect(cx, row_y, wide, cell_h, FB_RGB(0x2a, 0x34, 0x4a));
        fb_rect_outline(cx, row_y, wide, cell_h, C_WIN_BORDER);
        font_draw_string_scaled(cx + (wide - FONT_WIDTH * 2) / 2,
                                row_y + (cell_h - FONT_HEIGHT * 2) / 2,
                                "0", C_TEXT, 2);
        calc_btn& b = g_calc_btns[g_calc_btn_count++];
        b.x = cx; b.y = row_y; b.w = wide; b.h = cell_h;
        b.label[0] = '0'; b.label[1] = 0;
    }
    {
        int32_t cx = bx + cell_w * 2 + cell_pad * 2;
        fb_fill_rect(cx, row_y, wide, cell_h, FB_RGB(0x6a, 0xd2, 0x9a));
        fb_rect_outline(cx, row_y, wide, cell_h, C_WIN_BORDER);
        font_draw_string_scaled(cx + (wide - FONT_WIDTH * 2) / 2,
                                row_y + (cell_h - FONT_HEIGHT * 2) / 2,
                                "=", FB_RGB(0x10, 0x30, 0x20), 2);
        calc_btn& b = g_calc_btns[g_calc_btn_count++];
        b.x = cx; b.y = row_y; b.w = wide; b.h = cell_h;
        b.label[0] = '='; b.label[1] = 0;
    }
}

static bool calculator_handle_click(int32_t cx, int32_t cy) {
    for (int i = 0; i < g_calc_btn_count; i++) {
        const calc_btn& b = g_calc_btns[i];
        if (point_in(cx, cy, b.x, b.y, b.w, b.h)) {
            calc_press(b.label);
            return true;
        }
    }
    return false;
}

static void draw_window_content(const window_t& w) {
    switch (w.type) {
        case APP_TERMINAL:    draw_terminal_in(w);   break;
        case APP_FILES:       draw_files_in(w);      break;
        case APP_MONITOR:     draw_monitor_in(w);    break;
        case APP_ABOUT:       draw_about_in(w);      break;
        case APP_EDITOR:      draw_editor_in(w);     break;
        case APP_CALCULATOR:  draw_calculator_in(w); break;
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

static bool editor_handle_click(int32_t cx, int32_t cy) {
    auto in_btn = [&](btn_rect r) {
        return point_in(cx, cy, r.x, r.y, r.w, r.h);
    };
    if (in_btn(g_btn_save)) {
        if (g_editor_file && diskfs_save(g_editor_file)) {
            g_last_save_tick = timer_get_ticks();
        }
        return true;
    }
    return false;
}

static bool files_handle_click(int32_t cx, int32_t cy) {
    auto in_btn = [&](btn_rect r) {
        return point_in(cx, cy, r.x, r.y, r.w, r.h);
    };

    // Pop Terminal up first so the user actually sees Run output land.
    auto run_exec = [&](vfs_node* n) {
        if (find_window_of_type(APP_TERMINAL) < 0) open_app(APP_TERMINAL);
        else z_bring_to_top(find_window_of_type(APP_TERMINAL));
        vfs_run(n, g_args_buf);
    };

    if (in_btn(g_args_field)) {
        g_args_focused = true;
        return true;
    }
    g_args_focused = false;

    if (in_btn(g_btn_open)) {
        if (g_files_selected) {
            switch (g_files_selected->kind) {
                case VFS_DIR:  g_files_dir = g_files_selected;
                               g_files_selected = nullptr; break;
                case VFS_FILE: open_editor_for(g_files_selected); break;
                case VFS_EXEC: run_exec(g_files_selected); break;
            }
        }
        return true;
    }
    if (in_btn(g_btn_edit)) {
        if (g_files_selected && g_files_selected->kind == VFS_FILE)
            open_editor_for(g_files_selected);
        return true;
    }
    if (in_btn(g_btn_run)) {
        if (g_files_selected && g_files_selected->kind == VFS_EXEC)
            run_exec(g_files_selected);
        return true;
    }

    for (int i = 0; i < g_files_row_count; i++) {
        files_row r = g_files_rows[i];
        if (cy >= r.y && cy < r.y + g_files_row_h
                && cx >= g_files_row_x
                && cx < g_files_row_x + g_files_row_w) {
            if (r.node == g_files_dir->parent ||
                (r.node && r.node->kind == VFS_DIR)) {
                g_files_dir = r.node;
                g_files_selected = nullptr;
                return true;
            }
            g_files_selected = r.node;
            return true;
        }
    }
    return false;
}

static bool handle_left_press(int32_t cx, int32_t cy) {
    // Taskbar buttons raise the matching window (the only way to surface
    // one that's fully buried, given there's no Alt+Tab).
    int32_t taskbar_top = (int32_t)fb_height() - TASKBAR_H;
    if (cy >= taskbar_top) {
        for (int i = 0; i < z_count; i++) {
            int32_t bx, by, bw, bh;
            if (!taskbar_btn_rect(i, &bx, &by, &bw, &bh)) break;
            if (point_in(cx, cy, bx, by, bw, bh)) {
                z_bring_to_top(z_order[i]);
                return true;
            }
        }
        return false;
    }

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
            g_drag_idx = idx;
            g_drag_off_x = w.x - cx;
            g_drag_off_y = w.y - cy;
            return true;
        }
        if (w.type == APP_FILES)       files_handle_click(cx, cy);
        if (w.type == APP_EDITOR)      editor_handle_click(cx, cy);
        if (w.type == APP_CALCULATOR)  calculator_handle_click(cx, cy);
        return true;
    }
    for (int i = 0; i < N_ICONS; i++) {
        if (point_in(cx, cy, icons[i].x, icons[i].y, ICON_W, ICON_H)) {
            open_app(icons[i].app);
            return true;
        }
    }
    return false;
}

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

void gui_init() {
    cursor_compile();
    term_clear();

    for (int i = 0; i < MAX_WINDOWS; i++) windows[i].used = false;
    z_count = 0;

    g_files_dir      = vfs_root();
    g_files_selected = nullptr;
    g_editor_file    = nullptr;

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

bool gui_handle_key(char c) {
    // Args field beats z-order — clicking into it is the user's signal.
    if (g_args_focused) {
        if (c == '\b') {
            if (g_args_len > 0) {
                g_args_len--;
                g_args_buf[g_args_len] = 0;
                term.dirty = true;
            }
        } else if (c == '\n' || c == '\r') {
            g_args_focused = false;
            term.dirty = true;
        } else if (c >= 32 && c < 127 && g_args_len + 1 < FILES_ARGS_CAP) {
            g_args_buf[g_args_len++] = c;
            g_args_buf[g_args_len]   = 0;
            term.dirty = true;
        }
        return true;
    }

    // Editor takes keys when on top; everything else falls through to the shell.
    if (z_count == 0) return false;
    int top = z_order[z_count - 1];
    if (windows[top].type != APP_EDITOR || !g_editor_file) return false;

    if (c == '\b') {
        if (vfs_pop_char(g_editor_file)) term.dirty = true;
    } else if (c == '\n' || c == '\r') {
        vfs_append_char(g_editor_file, '\n');
        term.dirty = true;
    } else if (c >= 32 && c < 127) {
        vfs_append_char(g_editor_file, c);
        term.dirty = true;
    }
    return true;
}

void gui_tick() {
    mouse_event ev = mouse_consume_event();

    bool need = term.dirty;

    // Press, then drag-update, then release — release last so a press+release
    // arriving in the same tick still ends the drag.
    if ((ev.pressed & MOUSE_BUTTON_LEFT) && g_drag_idx < 0) {
        if (handle_left_press(ev.x, ev.y)) need = true;
    }
    if (g_drag_idx >= 0 && windows[g_drag_idx].used) {
        int32_t nx = ev.x + g_drag_off_x;
        int32_t ny = ev.y + g_drag_off_y;
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

    static int32_t last_x = -1, last_y = -1;
    if (ev.x != last_x || ev.y != last_y) {
        last_x = ev.x; last_y = ev.y;
        need = true;
    }

    // ~10 Hz repaint so clock/monitor/starfield look alive.
    static uint64_t last_paint_tick = 0;
    uint64_t now = timer_get_ticks();
    if (now - last_paint_tick >= 10) {
        last_paint_tick = now;
        need = true;
    }

    if (!need) return;
    term.dirty = false;
    gui_present();
}
