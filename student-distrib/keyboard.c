#include "lib.h"
#include "i8259.h"
#include "keyboard.h"
#include "kheap.h"
#include "graphics/graphics.h"

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

// Test variable lol
int rect_copy_key_pressed = 0;

//for open,close,read,write
#define RTC_PASS 0
#define RTC_FAIL -1

void clear_linebuffer() {
	memset(linebuffer, '\0', TERMINAL_SIZE);
}

/* KBDUS means US Keyboard Layout.
 * CREDIT TO http://www.osdever.net/bkerndev/Docs/keyboard.htm
 * where we got the following scancode to ascii mapping */

// Array mapping keyboard scan codes to ASCII characters
const unsigned char kbdus[TERMINAL_SIZE] = {
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
const unsigned char shift_kbdus[TERMINAL_SIZE] = {
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

	// Initialize line buffer
	for(i = 0; i < TERMINAL_SIZE;i++) {
	  linebuffer[i] = '\0';
	}
	// Update cursor to point at beginning
	update_cursor();
	// Say keyboard_init is done
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
	// Send EOI
	send_eoi(KEYBOARD_IRQ);

	unsigned char scancode;
	unsigned char key_down;

	/* Check for shift,ctrl,alt,CapsLock,Backspace, and tab*/
	unsigned char is_shift, is_ctrl, is_alt, is_caps_lock, is_backspace, is_tab, is_up, is_down;

	/* Used for loops later*/
	int i;

	/* Keeps track of if ctrl+L was clicked*/
	int ctrlL = 0;

	/* Read the scan code and check if it corresponds to a key press or a key release */
	scancode = inb(KEYBOARD_CONTROLLER_DATA_PORT);
	key_down = ((scancode & KEY_DOWN_MASK) == 0);

	/* Check if the keys are either shift, ctrl, alt, caps lock, tab, or backspace*/
	is_shift     = ((scancode & SCAN_CODE_MASK) == LEFT_SHIFT_CODE || (scancode & SCAN_CODE_MASK) == RIGHT_SHIFT_CODE);
	is_ctrl      = ((scancode & SCAN_CODE_MASK) == LEFT_CTRL_CODE);
	is_alt       = ((scancode & SCAN_CODE_MASK) == LEFT_ALT_CODE);
	is_caps_lock = ((scancode & SCAN_CODE_MASK) == CAPS_LOCK_CODE);
	is_backspace = ((scancode & SCAN_CODE_MASK) == BACKSPACE_CODE);
	is_tab       = ((scancode & SCAN_CODE_MASK) == TAB_CODE);
	is_up 		 = ((scancode & SCAN_CODE_MASK) == UP_CDOE);
	is_down		 = ((scancode & SCAN_CODE_MASK) == DOWN_CODE);
	// Set shift, ctrl, alt based on whether the key is down or up
	if (is_shift) SET_BIT(keyboard_key_status, SHIFT, key_down);
	if (is_ctrl)  SET_BIT(keyboard_key_status, CTRL,  key_down);
	if (is_alt)   SET_BIT(keyboard_key_status, ALT,   key_down);


	// Set caps lock to the opposite of what it was
	if (is_caps_lock && key_down)
		SET_BIT(keyboard_key_status, CAPS_LOCK, !(keyboard_key_status & CAPS_LOCK));

	/* Check for ctrl+ something command*/
	if ((keyboard_key_status & CTRL) != 0) {
		/* All CTRL+ commands HERE*/

		/* If CTRL+L
		   clear screen and put cursor in upper right corner*/
		if (kbdus[(scancode & SCAN_CODE_MASK)] == 'l') {
			set_color(V_BLACK,V_CYAN);
			clear();
			/* Print linebuffer*/
			for(i = 0; i <= linepos;i++) {
				/* Print 4 spaces for tab*/
				if (linebuffer[i] == '\t') {
					putc(' ');
					putc(' ');
					putc(' ');
					putc(' ');
				}
				/* Move 1 ahead b/c linebuffer[TERMINAL_SIZE-1] = '\n'*/
				else if (i == TERMINAL_SIZE-1) {
					increment_location();
					continue;
				}
				else
				  putc(linebuffer[i]);
			}
			// Set CTRL+L to pressed
			ctrlL = 1;
			/* In the end decrement location so blinking cursor
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

	if (character == '\n' && key_down) {
		enter_flag = 0;
		linepos = 0;
	}

	// BACKSPACE
	// Clear 1 byte back in memory
	if (is_backspace && key_down && linepos > 0 && linepos < TERMINAL_SIZE) {
		// If buffer is full, remove \n and previous char
		//   set color
		set_color(V_BLACK,V_CYAN);

		// If clearing tab, clear back four characters
		if (linebuffer[linepos-1] == '\t') {
			// Clear back 4 characters
			clear_char();
			clear_char();
			clear_char();
			clear_char();
		} else {
			// Call on clear_char
			clear_char();
		}

		// Clear keyboard buffer at linepos
		linebuffer[linepos - 1] = '\0';
		// Decrement linepos
		if (linepos > 0) {
			linepos--;
		}
	}

	if (key_down) {
		// Use the shifted set if it's alphabetical and uppercase
		//   OR if it's non alphabetical but the shift key is down
		if ((is_alphabetical && uppercase) || (!is_alphabetical && use_shift))
			character = shift_kbdus[scancode & SCAN_CODE_MASK];

		/* Only print the character if:
		 *    - character is not 0
		 *    - backspace is NOT being used
		 *    - terminal buffer is NOT full
		 */
		if (character && ctrlL == 0 && (!is_backspace) && (linepos < TERMINAL_SIZE - 1) && character != '\n') {
			// If it's a tab, print 4 spaces
			if (is_tab && key_down) {
				putc(' ');
				putc(' ');
				putc(' ');
				putc(' ');
			} else {
				putc(character);
			}
			// Store character into line
			linebuffer[linepos] = character;
			linepos++;
			update_cursor();
		}
		// If enter is pressed ONLY print the \n character
		else if (character == '\n' && key_down) {
			putc('\n');
		}
		/* If buffer is full, then put \n as last character and print a newline */
		else if (linepos == TERMINAL_SIZE - 1) {
			putc('\n');
			character = '\n';
			linebuffer[linepos] = character;
			enter_flag = 0;
			linepos = 0;
		}
	}
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
	if (!keyboard_init)
		init_keyboard();
	for(i = 0; i < TERMINAL_SIZE;i++) {
		linebuffer[i] = '\0';
	}
	update_cursor();
	return 0;
}

/*
 * closes vairables if needed
 * INPUTS: none
 * OUTPUTS: 0, to indicate it went well
 * SIDE EFFECTS: nothing
 */
int32_t terminal_close(void) {
	int32_t i;
	// Clear buffer
	for(i = 0; i < TERMINAL_SIZE;i++) {
		linebuffer[i] = '\0';
	}
	return 0;
}

/*
 * Initializes variables if needed and if keyboard has not been initialized, initialize it
 * INPUTS: int32_t fd = file descriptor
 *         char* buf = buffer to copy into
 *         int32_t bytes = number of bytes to copy into buf
 * OUTPUTS: 0, to indicate it went well
 * SIDE EFFECTS: modifies linebuffer
 */
int32_t terminal_read(int32_t fd, char* buf, int32_t bytes) {
	// Check for null buffer or a negative number of bytes, both of which are invalid
	if (buf == NULL || bytes < 0)
		return -1;
	// Nothing to do if 0 bytes are to be copied
	else if (bytes == 0)
		return 0;

	// Let the program spin while waiting for the keyboard handler to set the enter_flag
	while (enter_flag);
	enter_flag = 1;

	// Disable interrupts so that keyboard_handler doesn't touch linebuffer during read
	cli();

	// We will only copy up to TERMINAL_SIZE, since that is the maximum size of the linebuffer
	if (bytes > TERMINAL_SIZE)
		bytes = TERMINAL_SIZE;

	// Copy bytes into the buffer until we reach \0 or until we reach the end of buffer
	int i, bytes_copied;
	for (i = 0; i < bytes; i++) {
		buf[i] = linebuffer[i];

		if (linebuffer[i] == '\0') {
			buf[i] = '\n';
			break;
		}
	}
	bytes_copied = i + 1;

	// Shift over the rest of the line buffer that wasn't copied to index 0
	for (i = bytes_copied; i < TERMINAL_SIZE; i++) {
		linebuffer[i - bytes_copied] = linebuffer[i];
	}
	// Clear the rest of the line buffer
	for (i = TERMINAL_SIZE - bytes_copied; i < TERMINAL_SIZE; i++) {
		linebuffer[i] = '\0';
	}

	// Enable interrupts and return number of bytes copied
	sti();
	return bytes_copied;
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
	// If buf is NULL or bytes is negative, function can't complete
	if (buf == NULL || bytes < 0)
		return -1;
	// If it's 0, then 0 bytes are written to terminal, PASS
	if (bytes == 0)
		return 0;
	// Start new line before writing
	for (i = 0; i < bytes; i++) {
		// Should end at \0
		if (buf[i] != '\0') {
			putc(buf[i]);
		} else
			break;
	}
	// End with new line
	return 0;

}
