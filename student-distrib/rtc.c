#include "rtc.h"
#include "lib.h"
#include "i8259.h"
#include "rtc.h"

/* Whether or not user is waiting for interrupt */
volatile int interrupt = 0;

/* Indicates whether or not initialization has been called */
static int init = 0;

/*
 * NMI_enable()
 * Enables non-maskable interrupts
 *
 * INPUTS: none
 * OUTPUTS: none
 * SIDE EFFECTS: Enables the non-maskable interrupt
 */
void NMI_enable() {
	outb(inb(RTC_ADDRESS_PORT) & NMI_ENABLE_MASK, RTC_ADDRESS_PORT);
}
/*
 * NMI_disable()
 * Disable non-maskable interrupts
 *
 * INPUTS: none
 * OUTPUTS: none
 * SIDE EFFECTS: Disables the non-maskable interrupt
 */
void NMI_disable() {
	outb(inb(RTC_ADDRESS_PORT) | NMI_DISABLE_MASK, RTC_ADDRESS_PORT);
}

/*
 * init_rtc()
 * Initialies the RTC registers and enables the interrupt
 *
 * INPUTS: none
 * OUTPUTS: none
 * SIDE EFFECTS: Initializes the RTC
 */
void init_rtc() {
	/* Disables NMI to prevent undefined behavior */
	NMI_disable();

	/* Enables interrupt generation in RTC */
	outb(REGISTER_A, RTC_ADDRESS_PORT);
	outb(RTC_INTERRUPT_ENABLE_CMD, RTC_DATA_PORT);

	/* Turns on IR8 periodic interrupt */
	outb(REGISTER_B, RTC_ADDRESS_PORT);
	char prev_data = inb(RTC_DATA_PORT);
	outb(REGISTER_B, RTC_ADDRESS_PORT);
	outb(prev_data | 0x40, RTC_DATA_PORT);

	set_freq(_2HZ_);

	/* Enables NMI again */
	NMI_enable();

	/* Init keeps track of whether init_rtc() was called or not */
	init = 1;
	
	enable_irq(RTC_IRQ);
}

/*
 * set_freq()
 * Sets the frequency to what input says
 *
 * INPUTS: int32_t f: frequency RTC is
 *                    supposed to be set
 *                    to
 * OUTPUTS: -1 for FAIL, otherwise 0
 * SIDE EFFECTS: sets freq
 */
int32_t set_freq(int32_t f) {
	/* Use freq to store real freq byte to write */
	int32_t freq;
	/* Use switch to determine which freqency to set freq to */
	switch (f) {
		case 2:    freq = _2HZ_;    break;
		case 4:    freq = _4HZ_;    break;
		case 8:    freq = _8HZ_;    break;
		case 16:   freq = _16HZ_;   break;
		case 32:   freq = _32HZ_;   break;
		case 64:   freq = _64HZ_;   break;
		case 128:  freq = _128HZ_;  break;
		case 256:  freq = _256HZ_;  break;
		case 512:  freq = _512HZ_;  break;
		case 1024: freq = _1024HZ_; break;
		default: return -1;
	}

	/* Changing the interrupt rate */
	char prev_data;
	outb(REGISTER_A, RTC_ADDRESS_PORT);
	prev_data = inb(RTC_DATA_PORT);
	outb(REGISTER_A, RTC_ADDRESS_PORT);
	outb((prev_data & 0xF0) | freq, RTC_DATA_PORT);

	/* Return 4 bytes written */
	return 4;
}

/*
 * rtc_handler()
 * Handler which gets called every time RTC sends an interrupt
 *
 * INPUTS: none
 * OUTPUTS: none
 * SIDE EFFECTS: Handler for the RTC
 */
void rtc_handler() {
	cli();
	/* Select register C */
	outb(REGISTER_C, RTC_ADDRESS_PORT);
	/* Throw away contents they don't matter :( */
	inb(RTC_DATA_PORT);
	/* Send EOI */
	send_eoi(RTC_IRQ);
	/* Set user interrupt to 0 */
	interrupt = 0;

	sti();
}

/*
 * rtc_open()
 * Does necessary stuff to initialize/start rtc
 *
 * INPUTS: filename: unused
 * OUTPUTS: 0 for pass
 * SIDE EFFECTS: Initializes RTC if not initialized
 *               and sets freq to 2 HZ
 */
int32_t rtc_open(const uint8_t *filename) {
	/* Call init_rtc if not yet initialized */
	if (!init)
		init_rtc();

	/* Clear interrupts so nothing stops us from setting freq */
	cli();
	/* Set freq to 2 HZ by default */
	set_freq(2);
	/* Enable interrupts again */
	sti();
	/* There is no way for this to fail, so return 0 */
	return 0;
}

/*
 * rtc_close()
 * Does necessary stuff to close rtc
 *
 * INPUTS: fd: unused
 * OUTPUTS: 0 for pass
 * SIDE EFFECTS: N/A
 */
int32_t rtc_close(int32_t fd) {
	/* Nothing to do, so return 0 */
	return 0;
}

/*
 * rtc_read()
 * Waits for an RTC interrupt
 *
 * INPUTS: int32_t fd = file descriptor
 *         void* buf = buffer that's useless for now
 *         int32_t bytes = number of bytes in buf
 * OUTPUTS: 0 for pass
 * SIDE EFFECTS: Waits for RTC interrupt
 */
int32_t rtc_read(int32_t fd, void *buf, int32_t bytes) {
	/* Use user controlled flag for interrupt */
	interrupt = 1;

	/* Enable all interrupts, so that we get the RTC interrupt when it occurs */
	sti();

	/* While loop causes program to poll until interrupt is 0, which occurs when rtc_handler() is called */
	while (interrupt == 1);

	/* Everything done correctly, so return 0 */
	return 0;
}

/*
 * rtc_write()
 * Sets frequency to what's specified in buf
 *
 * INPUTS: int32_t fd = file descriptor
 *         const void* buf = buffer that contains frequency
 *         int32_t bytes = number of bytes in buf (should be 4)
 * OUTPUTS: 0 for pass
 * SIDE EFFECTS: Waits for RTC interrupt
 */
int32_t rtc_write(int32_t fd, const void *buf, int32_t bytes) {
	if (buf == NULL || bytes != 4)
		return -1;

	/* Read in buffer and cast it to freq */
	int32_t freq;
	freq = *(int32_t*)buf;
	/* Clear interrupts for setting freq */
	cli();
	/* Call set_freq to set frequency of RTC */
	int32_t ret = set_freq(freq);
	/* Enable interrupts again */
	sti();
	/* Return number of bytes written or -1 if bad input */
	return ret;
}
