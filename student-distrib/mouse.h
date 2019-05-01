#ifndef _MOUSE_H
#define _MOUSE_H

#include "types.h"

typedef struct mouse_info {
    uint32_t x;
    uint32_t y;
    uint32_t scroll;
    int right_click;
    int left_click;
} mouse_info;

extern mouse_info *mouse;

// Initializes the mouse to use interrupts and enables the mouse interrupt
void init_mouse();

// Interrupt handler for IRQ12 (mouse interrupt)
void mouse_handler();

void bound_mouse_coordinates();

void set_mouse_rate(int8_t rate);

#endif

