#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include "types.h"
#include "irq_defs.h"

/* Port numbers for keyboard controller status and data ports */
#define KEYBOARD_CONTROLLER_STATUS_PORT 0x64
#define KEYBOARD_CONTROLLER_DATA_PORT   0x60

/* Values sent to status port in order to write/read to controller command byte */
#define CCB_WRITE 0x60
#define CCB_READ  0x20

/* Keyboard controller command byte flags */
#define KEYBOARD_INTERRUPT_ENABLE 0x1
#define TRANSLATE_KEYBOARD_SCANCODE 0x40
#define DISABLE_MOUSE 0x20

/* Important scancodes */
#define LEFT_SHIFT_CODE  0x2A
#define RIGHT_SHIFT_CODE 0x36
#define CAPS_LOCK_CODE   0x3A
#define LEFT_CTRL_CODE   0x1D
#define LEFT_ALT_CODE    0x38
#define BACKSPACE_CODE   0x0E
#define TAB_CODE         0x0F

/* Keyboard key status flags */
#define SHIFT     0x1
#define CAPS_LOCK 0x2
#define CTRL      0x4
#define ALT       0x8

/* Important bitmasks */
#define KEY_DOWN_MASK 0x80
#define SCAN_CODE_MASK 0x7F
#define TERMINAL_SIZE 128

/* The number of spaces printed for a tab character */
#define NUM_SPACES_PER_TAB 4

// Initializes the keyboard to use interrupts and enables the keyboard interrupt
void init_keyboard();

// Interrupt handler for IRQ1 (keyboard interrupt)
void keyboard_handler();

// Initializes the terminal to be written or read from
extern int32_t terminal_open(void);
// Uninitializes the terminal from being written or read
extern int32_t terminal_close(void);
// Blocking call that returns the string typed after 127 characters or the enter key is pressed
extern int32_t terminal_read(int32_t fd, char* buf, int32_t bytes);
// Writes the provided string to the screen
extern int32_t terminal_write(int32_t fd, const char* buf, int32_t bytes);

#endif
