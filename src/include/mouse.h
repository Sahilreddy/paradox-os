// PS/2 mouse driver — 3-byte packet protocol via the second PS/2 channel.

#ifndef MOUSE_H
#define MOUSE_H

#include "types.h"

#define MOUSE_BUTTON_LEFT   0x01
#define MOUSE_BUTTON_RIGHT  0x02
#define MOUSE_BUTTON_MIDDLE 0x04

struct mouse_state {
    int32_t x, y;       // current cursor position (clipped to screen)
    uint8_t buttons;    // bitmask of MOUSE_BUTTON_*
    bool    moved;      // set true when state changed since last consume
};

// Initialize the controller, enable the auxiliary device, and unmask IRQ12.
// `screen_w`/`screen_h` clip the cursor inside the framebuffer.
void mouse_init(int32_t screen_w, int32_t screen_h);

// IRQ12 handler. Called from irq_handler() in idt.cpp.
void mouse_handler();

// Snapshot of the current cursor state. Clears `moved`.
mouse_state mouse_consume();

// Peek without clearing `moved`.
const mouse_state* mouse_peek();

// One mouse event derived from the current state vs. the previous tick:
// which buttons just transitioned down/up, and the cursor position at
// the moment of the transition. Used by the GUI for click handling.
struct mouse_event {
    int32_t x, y;
    uint8_t buttons;          // current button state
    uint8_t pressed;          // bits for transitions 0->1 since last call
    uint8_t released;         // bits for transitions 1->0 since last call
};

// Returns the event diff since the last call. Always safe to call.
mouse_event mouse_consume_event();

#endif
