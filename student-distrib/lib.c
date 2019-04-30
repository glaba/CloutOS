/* lib.c - Some basic library functions (printf, strlen, etc.)
 * vim:ts=4 noexpandtab */

#include "lib.h"
#include "processes.h"

#define NUM_COLS    80
#define NUM_ROWS    25

// Position of cursor for each TTY
static int screen_x[NUM_TEXT_TTYS];
static int screen_y[NUM_TEXT_TTYS];

// Color of text that is drawn in the future
static uint8_t attrib = 0x3;

/*
 * Clears the screen for the given TTY
 * INPUTS: tty: the TTY for which to clear the screen
 */
void clear_tty(uint8_t tty) {
	// Acquire the lock to write to video memory
	spin_lock_irqsave(tty_spin_lock);

	uint8_t *video_mem = (uint8_t*)get_vid_mem(tty);
	if (video_mem == NULL) {
		spin_unlock_irqsave(tty_spin_lock);
		return;
	}

	int32_t i;
	for (i = 0; i < NUM_ROWS * NUM_COLS; i++) {
		video_mem[i << 1] = ' ';
		video_mem[(i << 1) + 1] = attrib;
	}
	spin_unlock_irqsave(tty_spin_lock);

	// Reset the cursor to point to the top left of the screen
	screen_x[tty - 1] = 0;
	screen_y[tty - 1] = 0;
	
	update_cursor();
}

/*
 * Clears the screen for the currently active TTY
 */
void clear() {
	clear_tty(active_tty);
}

/*
 * Sets the color with which subsequent text will be drawn
 *
 * INPUTS: back_color, fore_color: the background and foreground color
 * OUTPUTS: none
 * SIDE EFFECTS: all future text drawn until another invocation of this function will
 *               be drawn with the given color
 */
void set_color(uint8_t back_color, uint8_t fore_color) {
	attrib = fore_color | (back_color << 4);
}

/*
 * Sets the location of the cursor
 *
 * INPUTS: (x, y): the updated location of the cursor where text will start being drawn
 * OUTPUTS: NONE
 * SIDE EFFECTS: screen_x and screen_y are updated
 */
void set_cursor_location(int x, int y) {
	spin_lock_irqsave(tty_spin_lock);

	screen_x[active_tty - 1] = x;
	screen_y[active_tty - 1] = y;

	spin_unlock_irqsave(tty_spin_lock);
}

/*
 * Decrements the cursor location by 1 for the given TTY
 *
 * INPUTS: tty: the TTY to update the cursor location within
 * SIDE EFFECTS: screen_x and screen_y are updated for the given TTY
 */
void decrement_location(uint8_t tty) {
	if (screen_x[tty - 1] <= 0 && screen_y[tty - 1] <= 0)
		return;
	screen_x[tty - 1]--;

	if (screen_x[tty - 1] < 0 && screen_y[tty - 1] > 0) {
		screen_y[tty - 1]--;
		screen_x[tty - 1] = NUM_COLS - 1;
	}
	update_cursor();
}

/*
 * Performs a single backspace operation
 *
 * INPUTS: tty: the TTY for which to go back one space
 * OUTPUTS: none
 * SIDE EFFECTS: clears 1 char back in memory,
 *               updates cursor
 */
void clear_char(uint8_t tty) {
	if (screen_x[tty - 1] <= 0 && screen_y[tty - 1] <= 0)
		return;

	spin_lock_irqsave(tty_spin_lock);

	uint8_t *video_mem = get_vid_mem(tty);

	// The linear location within the array of the character to be removed 
	int i = NUM_COLS * screen_y[tty - 1] + screen_x[tty - 1] - 1;

	// Write a space into this location, clearing it
	video_mem[i << 1] = ' ';
	video_mem[(i << 1) + 1] = attrib;

	spin_unlock_irqsave(tty_spin_lock);
	
	// Update the screen coordinates
	decrement_location(tty);
}

/* Standard printf().
 * Only supports the following format strings:
 * %%  - print a literal '%' character
 * %x  - print a number in hexadecimal
 * %u  - print a number as an unsigned integer
 * %d  - print a number as a signed integer
 * %c  - print a character
 * %s  - print a string
 * %#x - print a number in 32-bit aligned hexadecimal, i.e.
 *       print 8 hexadecimal digits, zero-padded on the left.
 *       For example, the hex number "E" would be printed as
 *       "0000000E".
 *       Note: This is slightly different than the libc specification
 *       for the "#" modifier (this implementation doesn't add a "0x" at
 *       the beginning), but I think it's more flexible this way.
 *       Also note: %x is the only conversion specifier that can use
 *       the "#" modifier to alter output. 
 *
 * INPUTS: tty: the TTY to print this to
 *         format: format string as described above
 *         varargs: typical printf format
 */
int32_t printf_tty(uint8_t tty, int8_t *format, ...) {
	/* Pointer to the format string */
	int8_t* buf = format;

	/* Stack pointer for the other parameters */
	int32_t* esp = (void *)&format;
	esp++;

	while (*buf != '\0') {
		switch (*buf) {
			case '%':
				{
					int32_t alternate = 0;
					buf++;

format_char_switch:
					/* Conversion specifiers */
					switch (*buf) {
						/* Print a literal '%' character */
						case '%':
							putc_tty('%', tty);
							break;

						/* Use alternate formatting */
						case '#':
							alternate = 1;
							buf++;
							/* Yes, I know gotos are bad.  This is the
							 * most elegant and general way to do this,
							 * IMHO. */
							goto format_char_switch;

						/* Print a number in hexadecimal form */
						case 'x':
							{
								int8_t conv_buf[64];
								if (alternate == 0) {
									itoa(*((uint32_t *)esp), conv_buf, 16);
									puts_tty(conv_buf, tty);
								} else {
									int32_t starting_index;
									int32_t i;
									itoa(*((uint32_t *)esp), &conv_buf[8], 16);
									i = starting_index = strlen(&conv_buf[8]);
									while(i < 8) {
										conv_buf[i] = '0';
										i++;
									}
									puts_tty(&conv_buf[starting_index], tty);
								}
								esp++;
							}
							break;

						/* Print a number in unsigned int form */
						case 'u':
							{
								int8_t conv_buf[36];
								itoa(*((uint32_t *)esp), conv_buf, 10);
								puts_tty(conv_buf, tty);
								esp++;
							}
							break;

						/* Print a number in signed int form */
						case 'd':
							{
								int8_t conv_buf[36];
								int32_t value = *((int32_t *)esp);
								if(value < 0) {
									conv_buf[0] = '-';
									itoa(-value, &conv_buf[1], 10);
								} else {
									itoa(value, conv_buf, 10);
								}
								puts_tty(conv_buf, tty);
								esp++;
							}
							break;

						/* Print a single character */
						case 'c':
							putc_tty((uint8_t) *((int32_t *)esp), tty);
							esp++;
							break;

						/* Print a NULL-terminated string */
						case 's':
							puts_tty(*((int8_t **)esp), tty);
							esp++;
							break;

						default:
							break;
					}

				}
				break;

			default:
				putc_tty(*buf, tty);
				break;
		}
		buf++;
	}

	return (buf - format);
}

/* 
 * Prints a string to the specified TTY
 *
 * INPUTS: s: pointer to a null terminated string
 *         tty: the TTY to output the string to
 */
int32_t puts_tty(int8_t* s, uint8_t tty) {
	register int32_t index = 0;
	while (s[index] != '\0') {
		putc_tty(s[index], tty);
		index++;
	}

	return index;
}

/* 
 * Prints a string to the currently active TTY
 *
 * INPUTS: s: pointer to a null terminated string
 */
int32_t puts(int8_t* s) {
	return puts_tty(s, active_tty);
}

/*
 * Prints an ASCII image with top right corner at (x, y)
 *
 * INPUTS: s: the ASCII image to print
 *         x, y: the top left coordinates of the image
 * OUTPUTS: none
 * SIDE EFFECTS: prints an image onto the screen, overwriting what may already be there
 */
void print_image(const char* s, unsigned int x, unsigned int y) {
	spin_lock_irqsave(tty_spin_lock);

	uint8_t *video_mem = (uint8_t*)get_vid_mem(active_tty);	
	unsigned int start_x = x;

	for (; *s != '\0'; s++) {
		// Go to next iteration if we are out of bounds
		if (y >= NUM_ROWS || x >= NUM_COLS)
			continue;

		// If there is a newline character, jump to the next line in the image
		if (*s == '\n' || *s == '\r') {
			y++;
			x = start_x;
		// Otherwise, draw the image
		} else {
			*(video_mem + ((NUM_COLS * y + x) << 1))     = *s;
			*(video_mem + ((NUM_COLS * y + x) << 1) + 1) = attrib;

			x++;
		}
	}

	spin_unlock_irqsave(tty_spin_lock);
}

/*
 * Scrolls all the text on the given TTY up by one line and sets the cursor position on the bottom
 *
 * INPUTS: tty: the TTY to scroll
 * OUTPUTS: none
 * SIDE EFFECTS: scrolls the text in the video memory upwards like a console
 */
static void scroll_screen_tty(uint8_t tty) {
	spin_lock_irqsave(tty_spin_lock);

	uint8_t *video_mem = (uint8_t*)get_vid_mem(tty);
	screen_y[tty - 1] = NUM_ROWS - 1;
	screen_x[tty - 1] = 0;
	int x, y;

	// Copy all the lines up one
	for (x = 0; x < NUM_COLS; x++) {
		for (y = 1; y < NUM_ROWS; y++) {
			*(video_mem + ((NUM_COLS * (y - 1) + x) << 1)) = *(video_mem + ((NUM_COLS * y + x) << 1));
			*(video_mem + ((NUM_COLS * (y - 1) + x) << 1) + 1) = *(video_mem + ((NUM_COLS * y + x) << 1) + 1);
		}
	}

	// Finally, clear the last line
	for (x = 0; x < NUM_COLS; x++) {
		*(video_mem + ((NUM_COLS * (NUM_ROWS - 1) + x) << 1)) = ' ';
		*(video_mem + ((NUM_COLS * (NUM_ROWS - 1) + x) << 1) + 1) = attrib;
	}

	spin_unlock_irqsave(tty_spin_lock);
}

/*
 * Increments the cursor location by 1 for the given TTY
 *
 * INPUTS: tty: the TTY for which to move the cursor
 * OUTPUTS: none
 * SIDE EFFECTS: screen_x and screen_y are updated
 */
void increment_location(uint8_t tty) {
	screen_x[tty - 1] = (screen_x[tty - 1] + 1) % NUM_COLS;

	if (screen_x[tty - 1] == 0)
		screen_y[tty - 1]++;

	// Scroll the screen if we have reached the bottom of the page
	if (screen_y[tty - 1] == NUM_ROWS)
		scroll_screen_tty(tty);

	update_cursor();
}

/* 
 * Prints a single character to the given TTY
 *
 * INPUTS: c: the character to print
 *         tty: the TTY to print the character to
 */
void putc_tty(uint8_t c, uint8_t tty) {
	spin_lock_irqsave(tty_spin_lock);

	uint8_t *video_mem = (uint8_t*)get_vid_mem(tty);

	// If it is a newline character, move the cursor down one and reset x
	if (c == '\n' || c == '\r') {
		screen_y[tty - 1]++;
		screen_x[tty - 1] = 0;
	} else {
		// Otherwise, set the desired text at the cursor position
		video_mem[(NUM_COLS * screen_y[tty - 1] + screen_x[tty - 1]) << 1] = c;
		video_mem[((NUM_COLS * screen_y[tty - 1] + screen_x[tty - 1]) << 1) + 1] = attrib;

		// Update the screen coordinates
		screen_x[tty - 1] = (screen_x[tty - 1] + 1) % NUM_COLS;

		if (screen_x[tty - 1] == 0)
			screen_y[tty - 1]++;
	}

	spin_unlock_irqsave(tty_spin_lock);

	// Scroll the screen if we have reached the bottom of the page
	if (screen_y[tty - 1] == NUM_ROWS) {
		scroll_screen_tty(tty);
	}
	update_cursor();
}

/* 
 * Prints a single character to the screen
 *
 * INPUTS: c: the character to print
 */
void putc(uint8_t c) {
	putc_tty(c, active_tty);
}

/* void update_cursor(void);
 * Inputs: N/A
 * Return Value: n/a
 * Function: Updates where blinking cursor is for the active TTY
 * Source: https://wiki.osdev.org/Text_Mode_Cursor
 */
void update_cursor() {
	spin_lock_irqsave(tty_spin_lock);

	uint16_t post = screen_y[active_tty - 1] * NUM_COLS + screen_x[active_tty - 1];

	outb(0x0F, 0x3D4);
	outb(post & 0xFF, 0x3D5);
	outb(0x0E, 0x3D4);
	outb((post >> 8) & 0xFF, 0x3D5);

	spin_unlock_irqsave(tty_spin_lock);
}


/* int8_t* itoa(uint32_t value, int8_t* buf, int32_t radix);
 * Inputs: uint32_t value = number to convert
 *            int8_t* buf = allocated buffer to place string in
 *          int32_t radix = base system. hex, oct, dec, etc.
 * Return Value: number of bytes written
 * Function: Convert a number to its ASCII representation, with base "radix" */
int8_t* itoa(uint32_t value, int8_t* buf, int32_t radix) {
	static int8_t lookup[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	int8_t *newbuf = buf;
	int32_t i;
	uint32_t newval = value;

	/* Special case for zero */
	if (value == 0) {
		buf[0] = '0';
		buf[1] = '\0';
		return buf;
	}

	/* Go through the number one place value at a time, and add the
	 * correct digit to "newbuf".  We actually add characters to the
	 * ASCII string from lowest place value to highest, which is the
	 * opposite of how the number should be printed.  We'll reverse the
	 * characters later. */
	while (newval > 0) {
		i = newval % radix;
		*newbuf = lookup[i];
		newbuf++;
		newval /= radix;
	}

	/* Add a terminating NULL */
	*newbuf = '\0';

	/* Reverse the string and return */
	return strrev(buf);
}

/* int8_t* strrev(int8_t* s);
 * Inputs: int8_t* s = string to reverse
 * Return Value: reversed string
 * Function: reverses a string s */
int8_t* strrev(int8_t* s) {
	register int8_t tmp;
	register int32_t beg = 0;
	register int32_t end = strlen(s) - 1;

	while (beg < end) {
		tmp = s[end];
		s[end] = s[beg];
		s[beg] = tmp;
		beg++;
		end--;
	}
	return s;
}

/* uint32_t strlen(const int8_t* s);
 * Inputs: const int8_t* s = string to take length of
 * Return Value: length of string s
 * Function: return length of string s */
uint32_t strlen(const int8_t* s) {
	register uint32_t len = 0;
	while (s[len] != '\0')
		len++;
	return len;
}


/* strlen_avoid_overflow(const int8_t* s, uint32_t max);
 * Inputs: const int8_t* s = string to take length of
 * 		   uint32_t max    = max posible len of string
 * Return Value: length of string s
 * Function: return length of string s */
uint32_t strnlen(const int8_t* s, uint32_t max) {
	register uint32_t len = 0;
	while (len < max && s[len] != '\0')
		len++;
	return len;
}

/* void* memset(void* s, int32_t c, uint32_t n);
 * Inputs:    void* s = pointer to memory
 *          int32_t c = value to set memory to
 *         uint32_t n = number of bytes to set
 * Return Value: new string
 * Function: set n consecutive bytes of pointer s to value c */
void* memset(void* s, int32_t c, uint32_t n) {
	c &= 0xFF;
	asm volatile ("                 \n\
			.memset_top:            \n\
			testl   %%ecx, %%ecx    \n\
			jz      .memset_done    \n\
			testl   $0x3, %%edi     \n\
			jz      .memset_aligned \n\
			movb    %%al, (%%edi)   \n\
			addl    $1, %%edi       \n\
			subl    $1, %%ecx       \n\
			jmp     .memset_top     \n\
			.memset_aligned:        \n\
			movw    %%ds, %%dx      \n\
			movw    %%dx, %%es      \n\
			movl    %%ecx, %%edx    \n\
			shrl    $2, %%ecx       \n\
			andl    $0x3, %%edx     \n\
			cld                     \n\
			rep     stosl           \n\
			.memset_bottom:         \n\
			testl   %%edx, %%edx    \n\
			jz      .memset_done    \n\
			movb    %%al, (%%edi)   \n\
			addl    $1, %%edi       \n\
			subl    $1, %%edx       \n\
			jmp     .memset_bottom  \n\
			.memset_done:           \n\
			"
			:
			: "a"(c << 24 | c << 16 | c << 8 | c), "D"(s), "c"(n)
			: "edx", "memory", "cc"
	);
	return s;
}

/* void* memset_word(void* s, int32_t c, uint32_t n);
 * Description: Optimized memset_word
 * Inputs:    void* s = pointer to memory
 *          int32_t c = value to set memory to
 *         uint32_t n = number of bytes to set
 * Return Value: new string
 * Function: set lower 16 bits of n consecutive memory locations of pointer s to value c */
void* memset_word(void* s, int32_t c, uint32_t n) {
	asm volatile ("                 \n\
			movw    %%ds, %%dx      \n\
			movw    %%dx, %%es      \n\
			cld                     \n\
			rep     stosw           \n\
			"
			:
			: "a"(c), "D"(s), "c"(n)
			: "edx", "memory", "cc"
	);
	return s;
}

/* void* memset_dword(void* s, int32_t c, uint32_t n);
 * Inputs:    void* s = pointer to memory
 *          int32_t c = value to set memory to
 *         uint32_t n = number of bytes to set
 * Return Value: new string
 * Function: set n consecutive memory locations of pointer s to value c */
void* memset_dword(void* s, int32_t c, uint32_t n) {
	asm volatile ("                 \n\
			movw    %%ds, %%dx      \n\
			movw    %%dx, %%es      \n\
			cld                     \n\
			rep     stosl           \n\
			"
			:
			: "a"(c), "D"(s), "c"(n)
			: "edx", "memory", "cc"
	);
	return s;
}

/* void* memcpy(void* dest, const void* src, uint32_t n);
 * Inputs:      void* dest = destination of copy
 *         const void* src = source of copy
 *              uint32_t n = number of byets to copy
 * Return Value: pointer to dest
 * Function: copy n bytes of src to dest */
void* memcpy(void* dest, const void* src, uint32_t n) {
	asm volatile ("                 \n\
			.memcpy_top:            \n\
			testl   %%ecx, %%ecx    \n\
			jz      .memcpy_done    \n\
			testl   $0x3, %%edi     \n\
			jz      .memcpy_aligned \n\
			movb    (%%esi), %%al   \n\
			movb    %%al, (%%edi)   \n\
			addl    $1, %%edi       \n\
			addl    $1, %%esi       \n\
			subl    $1, %%ecx       \n\
			jmp     .memcpy_top     \n\
			.memcpy_aligned:        \n\
			movw    %%ds, %%dx      \n\
			movw    %%dx, %%es      \n\
			movl    %%ecx, %%edx    \n\
			shrl    $2, %%ecx       \n\
			andl    $0x3, %%edx     \n\
			cld                     \n\
			rep     movsl           \n\
			.memcpy_bottom:         \n\
			testl   %%edx, %%edx    \n\
			jz      .memcpy_done    \n\
			movb    (%%esi), %%al   \n\
			movb    %%al, (%%edi)   \n\
			addl    $1, %%edi       \n\
			addl    $1, %%esi       \n\
			subl    $1, %%edx       \n\
			jmp     .memcpy_bottom  \n\
			.memcpy_done:           \n\
			"
			:
			: "S"(src), "D"(dest), "c"(n)
			: "eax", "edx", "memory", "cc"
	);
	return dest;
}

/* void* memmove(void* dest, const void* src, uint32_t n);
 * Description: Optimized memmove (used for overlapping memory areas)
 * Inputs:      void* dest = destination of move
 *         const void* src = source of move
 *              uint32_t n = number of byets to move
 * Return Value: pointer to dest
 * Function: move n bytes of src to dest */
void* memmove(void* dest, const void* src, uint32_t n) {
	asm volatile ("                             \n\
			movw    %%ds, %%dx                  \n\
			movw    %%dx, %%es                  \n\
			cld                                 \n\
			cmp     %%edi, %%esi                \n\
			jae     .memmove_go                 \n\
			leal    -1(%%esi, %%ecx), %%esi     \n\
			leal    -1(%%edi, %%ecx), %%edi     \n\
			std                                 \n\
			.memmove_go:                        \n\
			rep     movsb                       \n\
			"
			:
			: "D"(dest), "S"(src), "c"(n)
			: "edx", "memory", "cc"
	);
	return dest;
}

/* int32_t strncmp(const int8_t* s1, const int8_t* s2, uint32_t n)
 * Inputs: const int8_t* s1 = first string to compare
 *         const int8_t* s2 = second string to compare
 *               uint32_t n = number of bytes to compare
 * Return Value: A zero value indicates that the characters compared
 *               in both strings form the same string.
 *               A value greater than zero indicates that the first
 *               character that does not match has a greater value
 *               in str1 than in str2; And a value less than zero
 *               indicates the opposite.
 * Function: compares string 1 and string 2 for equality */
int32_t strncmp(const int8_t* s1, const int8_t* s2, uint32_t n) {
	int32_t i;
	for (i = 0; i < n; i++) {
		if ((s1[i] != s2[i]) || (s1[i] == '\0') /* || s2[i] == '\0' */) {

			/* The s2[i] == '\0' is unnecessary because of the short-circuit
			 * semantics of 'if' expressions in C.  If the first expression
			 * (s1[i] != s2[i]) evaluates to false, that is, if s1[i] ==
			 * s2[i], then we only need to test either s1[i] or s2[i] for
			 * '\0', since we know they are equal. */
			return s1[i] - s2[i];
		}
	}
	return 0;
}

/* int8_t* strcpy(int8_t* dest, const int8_t* src)
 * Inputs:      int8_t* dest = destination string of copy
 *         const int8_t* src = source string of copy
 * Return Value: pointer to dest
 * Function: copy the source string into the destination string */
int8_t* strcpy(int8_t* dest, const int8_t* src) {
	int32_t i = 0;
	while (src[i] != '\0') {
		dest[i] = src[i];
		i++;
	}
	dest[i] = '\0';
	return dest;
}

/* int8_t* strcpy(int8_t* dest, const int8_t* src, uint32_t n)
 * Inputs:      int8_t* dest = destination string of copy
 *         const int8_t* src = source string of copy
 *                uint32_t n = number of bytes to copy
 * Return Value: pointer to dest
 * Function: copy n bytes of the source string into the destination string */
int8_t* strncpy(int8_t* dest, const int8_t* src, uint32_t n) {
	int32_t i = 0;
	while (src[i] != '\0' && i < n) {
		dest[i] = src[i];
		i++;
	}
	while (i < n) {
		dest[i] = '\0';
		i++;
	}
	return dest;
}
