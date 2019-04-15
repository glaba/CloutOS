#include "pit.h"
#include "lib.h"
#include "i8259.h"
#include "irq_defs.h"

double time = 0.0;
double interval;

/*
 * Initializes the Programmable Interval Timer to generate interrupts at a frequency of very close to 69 Hz
 */
void init_pit() {
	// Enable interrupts for the PIT
	enable_irq(TIMER_IRQ);

	// Write the desired configuration to the PIT command port
	outb(PIT_CMD_CHANNEL_0 | PIT_CMD_ACCESS_MODE_LO_HI | PIT_CMD_MODE_2 | PIT_CMD_BINARY_MODE, PIT_CMD_REGISTER);

	// Write the countdown timer with the desired value
	// First write the bottom 8 bits
	outb(PIT_RELOAD_VALUE & 0xFF, PIT_CHANNEL_0_DATA_PORT);
	// Then the upper 8 bits
	outb((PIT_RELOAD_VALUE >> 8) & 0xFF, PIT_CHANNEL_0_DATA_PORT);

	// Set the interval to be 1/frequency
	interval = 1.0 / PIT_FREQUENCY;
}

/*
 * Handler for the timer interrupt
 */
void timer_handler() {
	send_eoi(TIMER_IRQ);

	time += interval;
}
