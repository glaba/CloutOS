#include "lib.h"
#include "i8259.h"
#include "keyboard.h"

// Sets a bit in bitfield based on set (sets the bit if set is truthy)
// The input flag should be a binary number containing only one 1
//  which indicates which bit to set
#define SET_BIT(bitfield, flag, set) \
do {                                 \
    if ((set))                       \
        bitfield |= (flag);          \
    else                             \
        bitfield &= (~(flag));       \
} while (0)

// Stores information about the current status of the keyboard (shift, caps lock, ctrl, alt)
static unsigned int keyboard_key_status = 0;

/* KBDUS means US Keyboard Layout. 
 * CREDIT TO http://www.osdever.net/bkerndev/Docs/keyboard.htm 
 * where we got the following scancode to ascii mapping */

// Array mapping keyboard scan codes to ASCII characters
const unsigned char kbdus[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',     /* 9 */
    '9', '0', '-', '=', '\b',                           /* Backspace */
    '\t',                                               /* Tab */
    'q', 'w', 'e', 'r',                                 /* 19 */
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',       /* Enter key */
    0,                                                  /* 29   - Control */
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',   /* 39 */
    '\'', '`',   0,                                     /* Left shift */
    '\\', 'z', 'x', 'c', 'v', 'b', 'n',                 /* 49 */
    'm', ',', '.', '/',   0,                            /* Right shift */
    '*',
    0,      /* Alt */
    ' ',    /* Space bar */
    0,      /* Caps lock */
    0,      /* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,      /* < ... F10 */
    0,      /* 69 - Num lock*/
    0,      /* Scroll Lock */
    0,      /* Home key */
    0,      /* Up Arrow */
    0,      /* Page Up */
    '-',
    0,      /* Left Arrow */
    0,
    0,      /* Right Arrow */
    '+',
    0,      /* 79 - End key*/
    0,      /* Down Arrow */
    0,      /* Page Down */
    0,      /* Insert Key */
    0,      /* Delete Key */
    0,   0,   0,
    0,      /* F11 Key */
    0,      /* F12 Key */
    0,      /* All other keys are undefined */
};

// Array mapping keyboard scan codes to ASCII characters when the shift key is pressed
const unsigned char shift_kbdus[128] = {
    0,  27, '!', '@', '#', '$', '%', '^', '&', '*',
    '(', ')', '_', '+', '\b', 
    '\t',   
    'Q', 'W', 'E', 'R',                                 /* 19 */
    'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',       /* Enter key */
    0,                                                  /* 29   - Control */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',   /* 39 */
    '\'', '~',   0,                                     /* Left shift */
    '\\', 'Z', 'X', 'C', 'V', 'B', 'N',                 /* 49 */
    'M', '<', '>', '?',   0,                            /* Right shift */
    '*',
    0,      /* Alt */
    ' ',    /* Space bar */
    0,      /* Caps lock */
    0,      /* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,      /* < ... F10 */
    0,      /* 69 - Num lock*/
    0,      /* Scroll Lock */
    0,      /* Home key */
    0,      /* Up Arrow */
    0,      /* Page Up */
    '-',
    0,      /* Left Arrow */
    0,
    0,      /* Right Arrow */
    '+',
    0,      /* 79 - End key*/
    0,      /* Down Arrow */
    0,      /* Page Down */
    0,      /* Insert Key */
    0,      /* Delete Key */
    0,   0,   0,
    0,      /* F11 Key */
    0,      /* F12 Key */
    0,      /* All other keys are undefined */
};

/*
 * Initializes keyboard and enables keyboard interrupt 
 *
 * INPUTS: none
 * OUTPUTS: none
 * SIDE EFFECTS: enables interrupts for keyboard 
 */
void init_keyboard() {
    unsigned char ccb;

    // Read value of CCB 
    outb(CCB_READ, KEYBOARD_CONTROLLER_STATUS_PORT);
    ccb = inb(KEYBOARD_CONTROLLER_DATA_PORT);

    // Update CCB to enable keyboard interrupts
    ccb |= KEYBOARD_INTERRUPT_ENABLE | TRANSLATE_KEYBOARD_SCANCODE | DISABLE_MOUSE;

    // Write update value to CCB
    outb(CCB_WRITE, KEYBOARD_CONTROLLER_STATUS_PORT);
    outb(ccb, KEYBOARD_CONTROLLER_DATA_PORT);

    // Enable interrupt after all setup is complete
    enable_irq(KEYBOARD_IRQ);
}

/*
 * Reads the scancode from the keyboard port and outputs to the console correctly
 *
 * INPUTS: none
 * OUTPUTS: none
 * SIDE EFFECTS: outputs a key to the console when the keyboard interrupt is fired
 */
void keyboard_handler() {
    unsigned char scancode;
    unsigned char key_down;
    
    unsigned char is_shift, is_ctrl, is_alt, is_caps_lock;

    // Read the scan code and check if it corresponds to a key press or a key release
    scancode = inb(KEYBOARD_CONTROLLER_DATA_PORT);
    key_down = ((scancode & 0x80) == 0);
    
    // Check if the keys are either shift, ctrl, alt, caps lock
    is_shift     = ((scancode & 0x7F) == LEFT_SHIFT_CODE || (scancode & 0x7F) == RIGHT_SHIFT_CODE);
    is_ctrl      = ((scancode & 0x7F) == LEFT_CTRL_CODE);
    is_alt       = ((scancode & 0x7F) == LEFT_ALT_CODE);
    is_caps_lock = ((scancode & 0x7F) == CAPS_LOCK_CODE);

    // Set shift, ctrl, alt based on whether the key is down or up 
    if (is_shift) SET_BIT(keyboard_key_status, SHIFT, key_down);
    if (is_ctrl)  SET_BIT(keyboard_key_status, CTRL,  key_down);
    if (is_alt)   SET_BIT(keyboard_key_status, ALT,   key_down);

    // Set caps lock to the opposite of what it was
    if (is_caps_lock && key_down)
        SET_BIT(keyboard_key_status, CAPS_LOCK, !(keyboard_key_status & CAPS_LOCK));

    // If the character is alphabetical:
    // The printed character should be uppercase if either shift or caps lock is on (but not both, hence the XOR)
    unsigned char uppercase = ((keyboard_key_status & SHIFT) != 0) ^ ((keyboard_key_status & CAPS_LOCK) != 0);

    // If the character is not alphabetical:
    // Whether or not to use the shifted character set
    unsigned char use_shift = ((keyboard_key_status & SHIFT) != 0); 

    // Print the character 
    char character = kbdus[scancode & 0x7F];
    char is_alphabetical = (character >= 'a' && character <= 'z');

    if (key_down) {
        // Use the shifted set if it's alphabetical and uppercase
        //   OR if it's non alphabetical but the shift key is down
        if ((is_alphabetical && uppercase) || (!is_alphabetical && use_shift))
            character = shift_kbdus[scancode & 0x7F];

        // Only print the character if it's not 0
        if (character)
            putc(character);
    }

    // Send EOI
    send_eoi(KEYBOARD_IRQ);
}
