#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "types.h"

// PS/2 Keyboard ports
#define KEYBOARD_DATA_PORT      0x60
#define KEYBOARD_STATUS_PORT    0x64
#define KEYBOARD_COMMAND_PORT   0x64

// Keyboard commands
#define KEYBOARD_CMD_SET_LED    0xED
#define KEYBOARD_CMD_ECHO       0xEE
#define KEYBOARD_CMD_SCANCODE   0xF0
#define KEYBOARD_CMD_IDENTIFY   0xF2
#define KEYBOARD_CMD_TYPEMATIC  0xF3
#define KEYBOARD_CMD_ENABLE     0xF4
#define KEYBOARD_CMD_DISABLE    0xF5
#define KEYBOARD_CMD_RESET      0xFF

// Special keys
#define KEY_ESCAPE      0x01
#define KEY_BACKSPACE   0x0E
#define KEY_TAB         0x0F
#define KEY_ENTER       0x1C
#define KEY_LCTRL       0x1D
#define KEY_LSHIFT      0x2A
#define KEY_RSHIFT      0x36
#define KEY_LALT        0x38
#define KEY_SPACE       0x39
#define KEY_CAPSLOCK    0x3A
#define KEY_F1          0x3B
#define KEY_F2          0x3C
#define KEY_F3          0x3D
#define KEY_F4          0x3E
#define KEY_F5          0x3F
#define KEY_F6          0x40
#define KEY_F7          0x41
#define KEY_F8          0x42
#define KEY_F9          0x43
#define KEY_F10         0x44

// Modifier flags
#define MOD_SHIFT       (1 << 0)
#define MOD_CTRL        (1 << 1)
#define MOD_ALT         (1 << 2)
#define MOD_CAPSLOCK    (1 << 3)

// Functions
void keyboard_init();
void keyboard_handler();
char keyboard_getchar();
bool keyboard_has_key();

#endif
