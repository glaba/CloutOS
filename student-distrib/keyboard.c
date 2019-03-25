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

/* Stores information about the current status of the keyboard (shift, caps lock, ctrl, alt)*/
static unsigned int keyboard_key_status = 0;

/*flag that is 1 until it enter is pressed*/
volatile unsigned int enter_flag = 1;

/* store the position of where to store next linebuffer character */
static unsigned int linepos = 0;

/*store whether keyboard has been initialized or not*/
static unsigned int keyboard_init = 0;

//line buffer to store everything typed into terminal
unsigned char linebuffer[(TERMINAL_SIZE)];

//for open,close,read,write
#define RTC_PASS 0
#define RTC_FAIL -1

/* KBDUS means US Keyboard Layout.
 * CREDIT TO http://www.osdever.net/bkerndev/Docs/keyboard.htm
 * where we got the following scancode to ascii mapping */

// Array mapping keyboard scan codes to ASCII characters
const unsigned char kbdus[128] = {
    0,  27,
    '1', '2', '3', '4', '5', '6', '7', '8','9', '0', '-', '=', '\b' /* Backspace */,
    '\t' /* Tab */,
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n' /* Enter key */,
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

/* Array mapping keyboard scan codes to ASCII characters when the shift key is pressed*/
const unsigned char shift_kbdus[128] = {
    0,  27,
    '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t',                                               /*Tab*/
    'Q', 'W', 'E', 'R',                                 /* 19 */
    'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',       /* Enter key */
    0,                                                  /* 29   - Control */
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',   /* 39 */
    '\"', '~',   0,                                     /* Left shift */
    '|', 'Z', 'X', 'C', 'V', 'B', 'N',                 /* 49 */
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
    int i;/*used for loop*/

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

    //initialize line buffer
    for(i = 0; i < 128;i++) {
      linebuffer[i] = '\0';
    }
    //update cursor to point at beginning
    update_cursor();
    //say keyboard_init is done
    keyboard_init = 1;
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

    /*Check for shift,ctrl,alt,CapsLock,Backspace, and tab*/
    unsigned char is_shift, is_ctrl, is_alt, is_caps_lock, is_backspace, is_tab;

    /*Used for loops later*/
    int i;

    /*keeps track of if ctrl+L was clicked*/
    int ctrlL = 0;

    /*Read the scan code and check if it corresponds to a key press or a key release */
    scancode = inb(KEYBOARD_CONTROLLER_DATA_PORT);
    key_down = ((scancode & KEY_DOWN_MASK) == 0);

    /*Check if the keys are either shift, ctrl, alt, caps lock, tab, or backspace*/
    is_shift     = ((scancode & SCAN_CODE_MASK) == LEFT_SHIFT_CODE || (scancode & SCAN_CODE_MASK) == RIGHT_SHIFT_CODE);
    is_ctrl      = ((scancode & SCAN_CODE_MASK) == LEFT_CTRL_CODE);
    is_alt       = ((scancode & SCAN_CODE_MASK) == LEFT_ALT_CODE);
    is_caps_lock = ((scancode & SCAN_CODE_MASK) == CAPS_LOCK_CODE);
    is_backspace = ((scancode & SCAN_CODE_MASK) == BACKSPACE_CODE);
    is_tab       = ((scancode & SCAN_CODE_MASK) == TAB_CODE);
    // Set shift, ctrl, alt based on whether the key is down or up
    if (is_shift) SET_BIT(keyboard_key_status, SHIFT, key_down);
    if (is_ctrl)  SET_BIT(keyboard_key_status, CTRL,  key_down);
    if (is_alt)   SET_BIT(keyboard_key_status, ALT,   key_down);

    // Set caps lock to the opposite of what it was
    if (is_caps_lock && key_down)
        SET_BIT(keyboard_key_status, CAPS_LOCK, !(keyboard_key_status & CAPS_LOCK));


    /*check for ctrl+ something command*/
    if( (keyboard_key_status & CTRL) != 0) {
        /*all CTRL+ commands HERE*/

        /*if CTRL+L
          clear screen and put cursor in upper right corner*/
        if(kbdus[(scancode & SCAN_CODE_MASK)] == 'l') {
            set_color(V_BLACK,V_CYAN);
            clear();
            /*print linebuffer*/
            for(i = 0; i <= linepos;i++) {
                /*print 3 spaces for tab*/
                if(linebuffer[i] == '\t') {
                    putc(' ');
                    putc(' ');
                    putc(' ');
                }
                /*move 1 ahead b/c linebuffer[127] = '\n'*/
                else if(i == 127) {
                    increment_location();
                    continue;
                }
                else
                  putc(linebuffer[i]);
            }
            //set CTRL+L to pressed
            ctrlL = 1;
            /*in the end decrement location so blinking cursor
              is at correct location*/
            decrement_location();

        }

    }



    // If the character is alphabetical:
    // The printed character should be uppercase if either shift or caps lock is on (but not both, hence the XOR)
    unsigned char uppercase = ((keyboard_key_status & SHIFT) != 0) ^ ((keyboard_key_status & CAPS_LOCK) != 0);

    // If the character is not alphabetical:
    // Whether or not to use the shifted character set
    unsigned char use_shift = ((keyboard_key_status & SHIFT) != 0);

    // Print the character
    char character = kbdus[scancode & SCAN_CODE_MASK];
    char is_alphabetical = (character >= 'a' && character <= 'z');
    if(character == '\n' && key_down)
        enter_flag = 0;

    /*BACKSPACE
      Clear 1 byte back in memory*/
    if(is_backspace && key_down && linepos >= 0 && linepos < 127 && linebuffer[linepos] != '\n') {
        /*if buffer is full, remove \n and previous char*/
        /*set color*/
        set_color(V_BLACK,V_CYAN);
        /*call on clear_char*/
        clear_char();
        /*clear keyboard buffer at linepos*/
        linebuffer[linepos] = '\0';
        /*decrement linepos*/
        if(linepos > 0) {
            linepos--;
        }
    }

    if (key_down) {

        // Use the shifted set if it's alphabetical and uppercase
        //   OR if it's non alphabetical but the shift key is down
        if((is_alphabetical && uppercase) || (!is_alphabetical && use_shift))
            character = shift_kbdus[scancode & SCAN_CODE_MASK];

        /* Only print the character if:
         *    - character is not 0
         *    - backspace is NOT being used
         *    - terminal buffer is NOT full
         */
        if (character && ctrlL == 0 && (!is_backspace) && (linepos < 127)) {
            //if it's a tab, print 3 spaces
            if(is_tab && key_down) {
                putc(' ');
                putc(' ');
                putc(' ');
            }
            else {
                putc(character);
            }
            //store character into line
            linebuffer[linepos] = character;
            linepos++;
            update_cursor();
        }
        /*if buffer is full, then put \n as last character*/
        else if(linepos == 127) {
            character = '\n';
            linebuffer[linepos] = character;
            enter_flag = 0;
        }

    }

    // Send EOI
    send_eoi(KEYBOARD_IRQ);
}

/*
 * initializes vairables if needed and if keyboard
 * has not been initialized, initialize it
 *
 * INPUTS: none
 * OUTPUTS: 0, to indicate it went well
 * SIDE EFFECTS: modifies linebuffer
 */
int32_t terminal_open(void) {
    int32_t i;
    if(!keyboard_init)
        init_keyboard();
    for(i = 0; i < 128;i++) {
        linebuffer[i] = '\0';
    }
    update_cursor();
    return TERMINAL_PASS;
}

/*
 * closes vairables if needed
 * INPUTS: none
 * OUTPUTS: 0, to indicate it went well
 * SIDE EFFECTS: nothing
 */
int32_t terminal_close(void) {
    return TERMINAL_PASS;
}

/*
 * initializes vairables if needed and if keyboard
 * has not been initialized, initialize it
 * INPUTS: int32_t fd = file descriptor
 *         char* buf = buffer to copy into
 *         int32_t bytes = number of bytes to copy into buf
 * OUTPUTS: 0, to indicate it went well
 * SIDE EFFECTS: modifies linwbuffer
 */
int32_t terminal_read(int32_t fd, char* buf, int32_t bytes) {
    /*if userspace to copy into is NULL, can't write anything
    into NULL.*/
    if(buf == NULL || bytes < 0)
        return TERMINAL_FAIL;
    /*if 0 bytes to copy, do nothing*/
    else if(bytes == 0)
        return TERMINAL_PASS;


    /*let the program spin until an enter has been pressed*/
    while(enter_flag) {}
    enter_flag = 1;
    /*if the bytes to read > TERMINAL_SIZE supported,
     just copy the the number of bytes possible.*/
    cli();
    if(bytes > TERMINAL_SIZE)
        bytes = TERMINAL_SIZE;
    /* if bytes to read is moe than wahts being read from keyboard,
       set bytes to what's been read so far*/
    if(bytes > linepos+1)
        bytes = linepos;
    /* loop over possible loop to copy into userspace buf */
    int i;
    for(i = 0; i < bytes;i++) {
        //if linebuffer reaches \n, buf should only
        //contain that much
        if(linebuffer[i] != '\n') {
            buf[i] = linebuffer[i];
        }
        else {
            //end with null terminate
            buf[i] = '\0';
            bytes = i;
            break;
        }

    }

    int index;
    /* clear remaining characters written into userspace*/
    for(i = 0, index = bytes+1; index < TERMINAL_SIZE;index++)
        linebuffer[i++] = linebuffer[index];
    //clear rest of linebuffer that has already been copied
    while(i < TERMINAL_SIZE) {
        linebuffer[i] = '\0';
        i++;
    }
    //SPECIAL CASE: when linepos & bytes are max, only subtract by bytes
    if(linepos == 127 && bytes == 127) {
        linepos-=bytes;
    }
    //else, decrement by bytes+1, where 1 represents the \n
    else {
        linepos-=(bytes+1);
    }

    //enable interrupts and return # of bytes
    sti();
    return bytes;
}

/*
 * outputs buf into terminal
 * INPUTS: int32_t fd = file descriptor
 *         char* buf = buffer to copy into
 *         int32_t bytes = number of bytes to copy into buf
 * OUTPUTS: 0, to indicate it went well
 * SIDE EFFECTS: outputs to terminal
 */
int32_t terminal_write(int32_t fd, const char* buf, int32_t bytes) {
    int i;
    //if buf is NULL or bytes is negative, function can't complete
    if(buf == NULL || bytes < 0)
        return TERMINAL_FAIL;
    //if it's 0, then 0 bytes are written to terminal, PASS
    if(bytes == 0)
        return TERMINAL_PASS;
    //start new line before writing
    //putc('\n');
    for(i = 0; i < bytes;i++) {
        /*should end at \n or at \0*/
        if(buf[i] != '\n' && buf[i] != '\0') {
            putc(buf[i]);
        }
        else
            break;

    }
    //end with new line
    return TERMINAL_PASS;

}
