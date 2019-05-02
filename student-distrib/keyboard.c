#include "lib.h"
#include "i8259.h"
#include "keyboard.h"
#include "processes.h"
#include "signals.h"
#include "spinlock.h"

// The keyboard shortcuts we have so far are
//  - Ctrl+L to clear the screen (1)
//  - Ctrl+C to send the SIGNAL_INTERRUPT signal (1)
//  - Alt+4 to switch to graphics tty (1)
//  - Alt+1,2,3 to switch TTYs (NUM_TEXT_TTYS)
#define NUM_KEYBOARD_SHORTCUTS (1 + 1 + 1 + NUM_TEXT_TTYS)

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

/* store the position of where to store next linebuffer character */
static unsigned int linepos[NUM_TEXT_TTYS];

/*store whether keyboard has been initialized or not*/
static unsigned int keyboard_init = 0;

//line buffer to store everything typed into terminal
unsigned char linebuffer[NUM_TEXT_TTYS][TERMINAL_SIZE];

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

// Defines a keyboard shortcut as well as the function to call when that shortcut is triggered
typedef struct keyboard_shortcut {
	// A bitmask of the same format as keyboard_key_status, but only with fields for ctrl, alt, and shift
	// Defines which modifier keys need to be held to trigger this shortcut
	unsigned int req_keyboard_status;
	// The ASCII character that must also be depressed (assuming the non-shift mapping from scancodes to ASCII)
	// A value of zero means no character need be pressed
	char character;
	// The FN key that must be pressed (going from 1 to 12)
	// A value of zero means no function key need be pressed
	char fn_key;
	// The function to call when the shortcut is pressed, taking the character and fn number as an argument
	void (*callback)(char, char);
} keyboard_shortcut;

// Prototypes for keyboard shortcut handlers
void ctrl_L_handler(char character, char fn_key);
void ctrl_C_handler(char character, char fn_key);
void tty_switch_handler(char character, char fn_key);

static keyboard_shortcut keyboard_shortcuts[NUM_KEYBOARD_SHORTCUTS] = {
	{.req_keyboard_status = CTRL, .character = 'l', .fn_key = 0, .callback = ctrl_L_handler},
	{.req_keyboard_status = CTRL, .character = 'c', .fn_key = 0, .callback = ctrl_C_handler},
	{.req_keyboard_status = ALT,  .character = 0,   .fn_key = 1, .callback = tty_switch_handler},
	{.req_keyboard_status = ALT,  .character = 0,   .fn_key = 2, .callback = tty_switch_handler},
	{.req_keyboard_status = ALT,  .character = 0,   .fn_key = 3, .callback = tty_switch_handler},
	{.req_keyboard_status = ALT,  .character = 0,   .fn_key = 4, .callback = tty_switch_handler}
};

struct spinlock_t terminal_lock = SPIN_LOCK_UNLOCKED;

/*
 * Initializes keyboard and enables keyboard interrupt
 *
 * INPUTS: none
 * OUTPUTS: none
 * SIDE EFFECTS: enables interrupts for keyboard
 */
void init_keyboard() {
	unsigned char ccb;
	int i, j;

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
	for (i = 0; i < TERMINAL_SIZE; i++) {
		for (j = 0; j < NUM_TEXT_TTYS; j++)
			linebuffer[j][i] = '\0';
	}

	// Initialize the line positions
	for (i = 0; i < NUM_TEXT_TTYS; i++) {
		linepos[i] = 0;
	}

	// Update cursor to point at beginning
	update_cursor();

	// Say keyboard_init is done
	keyboard_init = 1;
}

/* 
 * Clears the screen and prints the current contents of the linebuffer at the top
 * INPUTS: character: the character pressed (it is always L), which we ignore
 *         fn_key: the number of the function key pressed (none) which we ignore
 */
void ctrl_L_handler(char character, char fn_key) {
	clear();

	// Print the contents of the current linebuffer
	int i, j;
	for (i = 0; i <= linepos[active_tty - 1]; i++) {
		// Print some number of spaces for a tab character
		if (linebuffer[active_tty - 1][i] == '\t') {
			for (j = 0; j < NUM_SPACES_PER_TAB; j++)
				putc_tty(' ', active_tty);
		}
		// Move 1 ahead b/c linebuffer[active_tty - 1][TERMINAL_SIZE-1] = '\n'
		else if (i == TERMINAL_SIZE - 1) {
			increment_location(active_tty);
			continue;
		}
		// Simply print the character regularly
		else
			putc_tty(linebuffer[active_tty - 1][i], active_tty);
	}

	// Decrement the location by one to put the cursor in the right location
	decrement_location(active_tty);
}

/*
 * Sends a SIGNAL_INTERRUPT signal to the currently running process
 * INPUTS: character: the character pressed (it is always C), which we ignore
 *         fn_key: the number of the function key pressed (none) which we ignore 
 */
void ctrl_C_handler(char character, char fn_key) {
	// Make sure that the current TTY is a text TTY
	spin_lock_irqsave(tty_spin_lock);
	if (active_tty > NUM_TEXT_TTYS) {
		spin_unlock_irqsave(tty_spin_lock);
		return;
	}

	// Get the process that is active in the current TTY
	// We will also need to lock the pcbs array
	spin_lock_irqsave(pcb_spin_lock);

	// The active process in the current TTY may still be in state PROCESS_SLEEPING due to some
	//  blocking I/O, so we will determine which process is active by instead picking the one
	//  that has the longest chain going up to a root shell (which we check for by parent_pid < 0)
	int i;
	int active_proc_pid = -1;
	int longest_chain = -1;
	for (i = 0; i < pcbs.length; i++) {
		if (pcbs.data[i].pid >= 0 && pcbs.data[i].tty == active_tty) {
			// Find the length of the chain from this process back to the root shell
			int cur_chain_len = 0;
			int cur_pid = i;
			while (pcbs.data[cur_pid].parent_pid >= 0) {
				cur_chain_len++;
				cur_pid = pcbs.data[cur_pid].parent_pid;
			}
			// Update the active_proc_pid and longest_chain if this one is longer
			if (cur_chain_len > longest_chain) {
				longest_chain = cur_chain_len;
				active_proc_pid = i;
			}
		}
	}

	// Send the SIGNAL_INTERRUPT signal to this process
	if (active_proc_pid >= 0) {
		send_signal(active_proc_pid, SIGNAL_INTERRUPT, 0);
	}

	// Unlock the spinlocks and return
	spin_unlock_irqsave(pcb_spin_lock);
	spin_unlock_irqsave(tty_spin_lock);
	return;
}

/*
 * Switches to the TTY indicated by the character pressed
 * INPUTS: character: the character pressed (none)
 *         fn_key: the number of the function key pressed, which indicates the TTY to switch to
 */
void tty_switch_handler(char character, char fn_key) {
	// Validate that the fn_key is a number in range
	if (fn_key < 1 || fn_key > NUM_TEXT_TTYS + 1)
		return;

	// Switch to that TTY
	tty_switch(fn_key);
}

/*
 * Reads the scancode from the keyboard port and outputs to the console correctly
 *
 * INPUTS: none
 * OUTPUTS: none
 * SIDE EFFECTS: outputs a key to the console when the keyboard interrupt is fired
 */
void keyboard_handler() {
	// Mark that we are not in userspace
	in_userspace = 0;

	// Send EOI
	send_eoi(KEYBOARD_IRQ);

	unsigned char scancode;
	unsigned char key_down;

	/* Check for shift, ctrl, alt, CapsLock, Backspace, and tab*/
	unsigned char is_shift, is_ctrl, is_alt, is_caps_lock, is_backspace, is_tab;

	int i;

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

	// Set shift, ctrl, alt based on whether the key is down or up
	if (is_shift) SET_BIT(keyboard_key_status, SHIFT, key_down);
	if (is_ctrl)  SET_BIT(keyboard_key_status, CTRL,  key_down);
	if (is_alt)   SET_BIT(keyboard_key_status, ALT,   key_down);

	// Set caps lock to the opposite of what it was
	if (is_caps_lock && key_down)
		SET_BIT(keyboard_key_status, CAPS_LOCK, !(keyboard_key_status & CAPS_LOCK));

	// We no longer care about keyup, since we only use keyup to keep track of modifier keys
	if (!key_down)
		goto keyboard_handler_end;

	// Get the fn_key number, if it was a function key that was pressed
	char fn_key = 0;
	if ((scancode & SCAN_CODE_MASK) >= FN1_TO_10_START && (scancode & SCAN_CODE_MASK) < FN1_TO_10_START + 10)
		fn_key = (scancode & SCAN_CODE_MASK) - FN1_TO_10_START + 1;
	if ((scancode & SCAN_CODE_MASK) == FN11)
		fn_key = 11;
	if ((scancode & SCAN_CODE_MASK) == FN12)
		fn_key = 12;

	// Get the character
	char character = kbdus[scancode & SCAN_CODE_MASK];

	// Check for any keyboard shortcuts that should be specially handled
	//  and exit the function after handling them
	for (i = 0; i < NUM_KEYBOARD_SHORTCUTS; i++) {
		// Mask out any bits that correspond to modifiers that are not CTRL, ALT, and SHIFT
		unsigned int modifier_status = keyboard_key_status & (CTRL | ALT | SHIFT);

		// Check if the modifier keys are correct, and if the character and fn_key are correct
		if (keyboard_shortcuts[i].req_keyboard_status == modifier_status &&
			keyboard_shortcuts[i].character == character &&
			keyboard_shortcuts[i].fn_key == fn_key) {

			// Call the callback
			keyboard_shortcuts[i].callback(character, fn_key);
			goto keyboard_handler_end;
		}
	}

	// If either the character is a newline, or if the buffer is full, we have the same behavior
	//  of flushing the line buffer and printing a newline
	if (character == '\n' || linepos[active_tty - 1] == TERMINAL_SIZE - 1) {
		putc_tty('\n', active_tty);

		linebuffer[active_tty - 1][linepos[active_tty - 1]] = '\0';
		linepos[active_tty - 1] = 0;

		// Look through all processes to find any that are in the current TTY and blocking on terminal read
		for (i = 0; i < pcbs.length; i++) {
			if (pcbs.data[i].pid >= 0 && pcbs.data[i].tty == active_tty && 
				pcbs.data[i].blocking_call.type == BLOCKING_CALL_TERMINAL_READ) {
				// Wake up the process, because it has received data
				process_wake(i);
				break;
			}
		}

		// Return, since we have printed the character already
		goto keyboard_handler_end;
	}

	// Now, we are printing regularly to the screen rather than handling special cases

	// Check whether or not the character is alphabetical for caps lock
	char is_alphabetical = (character >= 'a' && character <= 'z');

	// If the character is alphabetical:
	// The printed character should be uppercase if either shift or caps lock is on (but not both, hence the XOR)
	unsigned char uppercase = ((keyboard_key_status & SHIFT) != 0) ^ ((keyboard_key_status & CAPS_LOCK) != 0);

	// If the character is not alphabetical:
	// Whether or not to use the shifted character set
	unsigned char use_shift = ((keyboard_key_status & SHIFT) != 0);

	// Handle the special case of a backspace
	if (is_backspace && linepos[active_tty - 1] < TERMINAL_SIZE) {
		// Do nothing if we are already at the beginning
		if (linepos[active_tty - 1] == 0)
			goto keyboard_handler_end;

		// If clearing tab, clear back four characters
		if (linebuffer[active_tty - 1][linepos[active_tty - 1] - 1] == '\t') {
			// Clear back 4 spaces
			for (i = 0; i < NUM_SPACES_PER_TAB; i++)
				clear_char(active_tty);
		} else {
			// Clear back only 1 character
			clear_char(active_tty);
		}

		// Clear keyboard buffer at linepos
		linebuffer[active_tty - 1][linepos[active_tty - 1] - 1] = '\0';
		
		// Decrement linepos if we can go backwards
		if (linepos[active_tty - 1] > 0)
			linepos[active_tty - 1]--;

		goto keyboard_handler_end;
	}

	// Use the shifted set if it's alphabetical and uppercase
	//   OR if it's non alphabetical but the shift key is down
	if ((is_alphabetical && uppercase) || (!is_alphabetical && use_shift))
		character = shift_kbdus[scancode & SCAN_CODE_MASK];

	// Finally, we are just printing characters regularly
	// Only print the character if the character is non-zero
	if (character) {
		// If it's a tab, print 4 spaces
		if (is_tab) {
			for (i = 0; i < NUM_SPACES_PER_TAB; i++)
				putc_tty(' ', active_tty);
		} else {
			putc_tty(character, active_tty);
		}

		// Store character into line buffer and move the cursor over
		linebuffer[active_tty - 1][linepos[active_tty - 1]] = character;
		linepos[active_tty - 1]++;
		update_cursor();
	
		goto keyboard_handler_end;
	}

keyboard_handler_end:
	in_userspace = 1;
	return;
}

/*
 * Initializes vairables if needed and if keyboard
 * has not been initialized, initializes it
 *
 * INPUTS: none
 * OUTPUTS: 0, to indicate it went well
 * SIDE EFFECTS: modifies linebuffer
 */
int32_t terminal_open(const uint8_t* filename) {
	// Never actually used
	(void) filename;
	if (!keyboard_init)
		init_keyboard();

	spin_lock_irqsave(pcb_spin_lock);
	// Get the TTY of the calling process
	uint8_t tty = get_pcb()->tty;
	spin_unlock_irqsave(pcb_spin_lock);

	spin_lock_irqsave(terminal_lock);
	int i;
	for (i = 0; i < TERMINAL_SIZE; i++)
		linebuffer[tty - 1][i] = '\0';
	spin_unlock_irqsave(terminal_lock);

	update_cursor();
	return 0;
}

/*
 * closes vairables if needed
 * INPUTS: none
 * OUTPUTS: 0, to indicate it went well
 * SIDE EFFECTS: nothing
 */
int32_t terminal_close(int32_t fd) {
	(void) fd;
	int32_t i;

	// Clear buffer, blocking keyboard interrupts while this happenss
	spin_lock_irqsave(pcb_spin_lock);
	spin_lock_irqsave(terminal_lock);
	uint8_t tty = get_pcb()->tty;
	for (i = 0; i < TERMINAL_SIZE; i++)
		linebuffer[tty - 1][i] = '\0';
	spin_unlock_irqsave(terminal_lock);
	spin_unlock_irqsave(pcb_spin_lock);

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
	// Block context switch while we access the PCB
	spin_lock_irqsave(pcb_spin_lock);

	// Get the current TTY and save it
	pcb_t *pcb = get_pcb();
	uint8_t tty = pcb->tty;

	// Check for null buffer or a negative number of bytes, both of which are invalid
	if (buf == NULL || bytes < 0) {
		spin_unlock_irqsave(pcb_spin_lock);
		return -1;
	}
	// Nothing to do if 0 bytes are to be copied
	else if (bytes == 0) {
		spin_unlock_irqsave(pcb_spin_lock);
		return 0;
	}

	// Print out the contents of the line buffer that are there so far
	int i;
	for (i = 0; i < linepos[tty - 1]; i++) {
		putc_tty(linebuffer[tty - 1][i], tty);
	}

	// Restore interrupts now that we're done with using the PCB
	spin_unlock_irqsave(pcb_spin_lock);

	// Set the blocking call field in the PCB
	pcb->blocking_call.type = BLOCKING_CALL_TERMINAL_READ;

	sti();
	// Put the process to sleep
	process_sleep(pcb->pid);

	// Execution will return to here when the process is woken up
	// Disable interrupts so that keyboard_handler doesn't touch linebuffer during read
	spin_lock_irqsave(terminal_lock);

	// We will only copy up to TERMINAL_SIZE, since that is the maximum size of the linebuffer
	if (bytes > TERMINAL_SIZE)
		bytes = TERMINAL_SIZE;

	// Copy bytes into the buffer until we reach \0 or until we reach the end of buffer
	int bytes_copied;
	for (i = 0; i < bytes; i++) {
		buf[i] = linebuffer[tty - 1][i];

		if (linebuffer[tty - 1][i] == '\0') {
			buf[i] = '\n';
			break;
		}
	}
	bytes_copied = i + 1;

	// Shift over the rest of the line buffer that wasn't copied to index 0
	for (i = bytes_copied; i < TERMINAL_SIZE; i++) {
		linebuffer[tty - 1][i - bytes_copied] = linebuffer[tty - 1][i];
	}
	// Clear the rest of the line buffer
	for (i = TERMINAL_SIZE - bytes_copied; i < TERMINAL_SIZE; i++) {
		linebuffer[tty - 1][i] = '\0';
	}

	// Enable interrupts and return number of bytes copied
	spin_unlock_irqsave(terminal_lock);
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
	spin_lock_irqsave(pcb_spin_lock);

	// Get the PCB for the current process
	// We are mainly interested in getting the current TTY
	pcb_t *pcb = get_pcb();
	uint8_t tty = pcb->tty;

	spin_unlock_irqsave(pcb_spin_lock);

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
			putc_tty(buf[i], tty);
		} else
			break;
	}
	// End with new line
	return 0;
}
