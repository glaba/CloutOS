#ifndef KEYBOARD_H
#define KEYBOARD_H


#define KEYBOARD_CONTROLLER_STATUS_PORT 0x64
#define KEYBOARD_CONTROLLER_DATA_PORT   0x60
#define CCB_WRITE                 0x60
#define CCB_READ                  0x20
#define KEYBOARD_INTERRUPT_ENABLE 0x1
#define KEYBOARD_IRQ 1
#define TRANSLATE_KEYBOARD_SCANCODE 0x40
#define DISABLE_MOUSE 0x20

void init_keyboard();
void keyboard_handler();

#endif

