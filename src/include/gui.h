// Compositor: boot splash, desktop with clickable icons, multiple
// stacked windows (Terminal / Files / About), and a mouse cursor overlay.

#ifndef GUI_H
#define GUI_H

#include "types.h"

// Build the desktop state (icons + empty window list). Call after the
// framebuffer + mouse are initialized but before redirecting shell text.
void gui_init();

// Block-and-animate the boot splash for ~2 seconds. Uses the timer for
// pacing. Call after gui_init.
void gui_run_splash();

// Push the back buffer to the hardware framebuffer.
void gui_present();

// Terminal pipe — the shell writes here. The text is stored in a
// persistent grid even when no Terminal window is open, so opening one
// later shows the full history.
void gui_term_putc(char c);
void gui_term_puts(const char* s);

// Polled from the main loop. Reads mouse events (clicks, motion) and
// repaints whenever the screen state has changed.
void gui_tick();

#endif
