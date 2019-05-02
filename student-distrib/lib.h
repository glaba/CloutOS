/* lib.h - Defines for useful library functions
 * vim:ts=4 noexpandtab
 */

#ifndef _LIB_H
#define _LIB_H

#include "types.h"
#include "processes.h"

// Start address of video memory and size of video memory in bytes
#define VIDEO      0xB8000
#define VIDEO_SIZE 0x08000

// Colors in video memory
#define V_BLACK        0x0
#define V_BLUE         0x1
#define V_GREEN        0x2
#define V_CYAN         0x3
#define V_RED          0x4
#define V_PURPLE       0x5
#define V_BROWN        0x6
#define V_GRAY         0x7
#define V_DARK_GRAY    0x8
#define V_LIGHT_BLUE   0x9
#define V_LIGHT_GREEN  0xA
#define V_LIGHT_CYAN   0xB
#define V_LIGHT_RED    0xC
#define V_LIGHT_PURPLE 0xD
#define V_YELLOW       0xE
#define V_WHITE        0xF

// A useful constant...
#define NULL 0

// Use a #define for printf to automatically transfer the variable arguments
#define printf(f, ...) (printf_tty(active_tty, f, ##__VA_ARGS__))

// Sets the color with which subsequent text will be drawn
void set_color(uint8_t back_color, uint8_t fore_color);

// Sets the location of the cursor
void set_cursor_location(int x, int y);
// Updates the text cursor location
void update_cursor();

// Prints an ASCII image with top right corner at (x, y)
void print_image(const char* s, unsigned int x, unsigned int y);

int32_t printf_tty(uint8_t tty, int8_t *format, ...);
void putc(uint8_t c);
void putc_tty(uint8_t c, uint8_t tty);
int32_t puts(int8_t *s);
int32_t puts_tty(int8_t *s, uint8_t tty);
int8_t *itoa(uint32_t value, int8_t* buf, int32_t radix);
int8_t *strrev(int8_t* s);
uint32_t strlen(const int8_t* s);
uint32_t strnlen(const int8_t* s, uint32_t max);
void clear_tty(uint8_t tty);
void clear();

void clear_char(uint8_t tty);

// decrement/increment the location by 1
void decrement_location(uint8_t tty);
void increment_location(uint8_t tty);

void* memset(void* s, int32_t c, uint32_t n);
void* memset_word(void* s, int32_t c, uint32_t n);
void* memset_dword(void* s, int32_t c, uint32_t n);
void* memcpy(void* dest, const void* src, uint32_t n);
void* memmove(void* dest, const void* src, uint32_t n);
int32_t strncmp(const int8_t* s1, const int8_t* s2, uint32_t n);
int8_t* strcpy(int8_t* dest, const int8_t*src);
int8_t* strncpy(int8_t* dest, const int8_t*src, uint32_t n);

/* Userspace address-check functions */
int32_t bad_userspace_addr(const void* addr, int32_t len);
int32_t safe_strncpy(int8_t* dest, const int8_t* src, int32_t n);

// Whether or not we are using VGA text mode (as opposed to VGA graphics mode)
extern int vga_text_enabled;

/* Port read functions */
/* Inb reads a byte and returns its value as a zero-extended 32-bit
 * unsigned int */
static inline uint32_t inb(port) {
	uint32_t val;
	asm volatile ("             \n\
			xorl %0, %0         \n\
			inb  (%w1), %b0     \n\
			"
			: "=a"(val)
			: "d"(port)
			: "memory"
	);
	return val;
}

/* Reads two bytes from two consecutive ports, starting at "port",
 * concatenates them little-endian style, and returns them zero-extended
 * */
static inline uint32_t inw(port) {
	uint32_t val;
	asm volatile ("             \n\
			xorl %0, %0         \n\
			inw  (%w1), %w0     \n\
			"
			: "=a"(val)
			: "d"(port)
			: "memory"
	);
	return val;
}

/* Reads four bytes from four consecutive ports, starting at "port",
 * concatenates them little-endian style, and returns them */
static inline uint32_t inl(port) {
	uint32_t val;
	asm volatile ("inl (%w1), %0"
			: "=a"(val)
			: "d"(port)
			: "memory"
	);
	return val;
}

/* Writes a byte to a port */
#define outb(data, port)                \
do {                                    \
	asm volatile ("outb %b1, (%w0)"     \
			:                           \
			: "d"(port), "a"(data)      \
			: "memory", "cc"            \
	);                                  \
} while (0)

/* Writes two bytes to two consecutive ports */
#define outw(data, port)                \
do {                                    \
	asm volatile ("outw %w1, (%w0)"     \
			:                           \
			: "d"(port), "a"(data)      \
			: "memory", "cc"            \
	);                                  \
} while (0)

/* Writes four bytes to four consecutive ports */
#define outl(data, port)                \
do {                                    \
	asm volatile ("outl %k1, (%w0)"     \
			:                           \
			: "d"(port), "a"(data)      \
			: "memory", "cc"            \
	);                                  \
} while (0)

/* Clear interrupt flag - disables interrupts on this processor */
#define cli()                           \
do {                                    \
	asm volatile ("cli"                 \
			:                           \
			:                           \
			: "memory", "cc"            \
	);                                  \
} while (0)

/* Save flags and then clear interrupt flag
 * Saves the EFLAGS register into the variable "flags", and then
 * disables interrupts on this processor */
#define cli_and_save(flags)             \
do {                                    \
	asm volatile ("                   \n\
			pushfl                    \n\
			popl %0                   \n\
			cli                       \n\
			"                           \
			: "=r"(flags)               \
			:                           \
			: "memory", "cc"            \
	);                                  \
} while (0)

/* Set interrupt flag - enable interrupts on this processor */
#define sti()                           \
do {                                    \
	asm volatile ("sti"                 \
			:                           \
			:                           \
			: "memory", "cc"            \
	);                                  \
} while (0)

/* Restore flags
 * Puts the value in "flags" into the EFLAGS register.  Most often used
 * after a cli_and_save_flags(flags) */
#define restore_flags(flags)            \
do {                                    \
	asm volatile ("                   \n\
			pushl %0                  \n\
			popfl                     \n\
			"                           \
			:                           \
			: "r"(flags)                \
			: "memory", "cc"            \
	);                                  \
} while (0)

#endif /* _LIB_H */
