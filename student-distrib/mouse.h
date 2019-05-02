#ifndef _MOUSE_H
#define _MOUSE_H

#include "types.h"

typedef struct mouse_info {
    uint32_t x;
    uint32_t y;
    uint32_t old_x;
    uint32_t old_y;
    uint32_t scroll;
    int right_click;
    int left_click;

    int holding_window;

} mouse_info;

extern mouse_info mouse;

// Reads the 5 bytes of mouse data (window ID, relative X, relative Y, left button, right button)
int32_t mouse_driver_read(int32_t fd, char *buf, int32_t bytes);

// Initializes the mouse to use interrupts and enables the mouse interrupt
void init_mouse();

// Interrupt handler for IRQ12 (mouse interrupt)
void mouse_handler();

void bound_mouse_coordinates();

void set_mouse_rate(int8_t rate);

#endif

