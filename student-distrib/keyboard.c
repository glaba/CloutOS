#include "lib.h"
#include "i8259.h"
#include "keyboard.h"

/* KBDUS means US Keyboard Layout. 
 * CREDIT TO http://www.osdever.net/bkerndev/Docs/keyboard.htm 
 * where we got the following scancode to ascii mapping */
unsigned char kbdus[128] =
{
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
  '9', '0', '-', '=', '\b',	/* Backspace */
  '\t',			/* Tab */
  'q', 'w', 'e', 'r',	/* 19 */
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
    0,			/* 29   - Control */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
 '\'', '`',   0,		/* Left shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
  'm', ',', '.', '/',   0,				/* Right shift */
  '*',
    0,	/* Alt */
  ' ',	/* Space bar */
    0,	/* Caps lock */
    0,	/* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,	/* < ... F10 */
    0,	/* 69 - Num lock*/
    0,	/* Scroll Lock */
    0,	/* Home key */
    0,	/* Up Arrow */
    0,	/* Page Up */
  '-',
    0,	/* Left Arrow */
    0,
    0,	/* Right Arrow */
  '+',
    0,	/* 79 - End key*/
    0,	/* Down Arrow */
    0,	/* Page Down */
    0,	/* Insert Key */
    0,	/* Delete Key */
    0,   0,   0,
    0,	/* F11 Key */
    0,	/* F12 Key */
    0,	/* All other keys are undefined */
};

void init_keyboard(){

    unsigned char ccb;

	// Read value of CCB 
	outb(CCB_READ, KEYBOARD_CONTROLLER_STATUS_PORT);
	ccb = inb(KEYBOARD_CONTROLLER_DATA_PORT);

	// Update CCB to enable keyboard interrupts
	ccb |= KEYBOARD_INTERRUPT_ENABLE | TRANSLATE_KEYBOARD_SCANCODE | DISABLE_MOUSE;

	// Write update value to CCB
	outb(CCB_WRITE, KEYBOARD_CONTROLLER_STATUS_PORT);
	outb(ccb, KEYBOARD_CONTROLLER_DATA_PORT);

    enable_irq(KEYBOARD_IRQ);
}

void keyboard_handler(){
    unsigned char scancode;
    scancode = inb(KEYBOARD_CONTROLLER_DATA_PORT);
    if (scancode & 0x80){}
    else{
        
        putc(kbdus[scancode]);
    }
    send_eoi(KEYBOARD_IRQ);
}

