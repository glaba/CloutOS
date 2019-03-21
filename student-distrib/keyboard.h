#ifndef _KEYBOARD_H
#define _KEYBOARD_H

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

/* Keyboard key status flags */
#define SHIFT     0x1
#define CTRL      0x4
#define ALT       0x8
#define CAPS_LOCK 0x2

// Initializes the keyboard to use interrupts and enables the keyboard interrupt 
void init_keyboard();

// Interrupt handler for IRQ1 (keyboard interrupt)
void keyboard_handler();

#endif
