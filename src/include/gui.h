// Compositor: splash, desktop, windows, mouse cursor.

#ifndef GUI_H
#define GUI_H

#include "types.h"

void gui_init();
void gui_run_splash();
void gui_present();

// vga_print is redirected here when graphical; opening a Terminal window
// later shows whatever scrolled past.
void gui_term_putc(char c);
void gui_term_puts(const char* s);

void gui_tick();

// Returns true if the GUI consumed the key (editor / args field had focus).
bool gui_handle_key(char c);

#endif
