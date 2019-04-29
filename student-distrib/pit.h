#ifndef _PIT_H
#define _PIT_H

#include "types.h"

// I/O ports for the Programmable Interval Timer
//  The data ports are used to read/write the current value of the countdown for each channel
//  The command register is used to set the mode of each channel
#define PIT_CHANNEL_0_DATA_PORT 0x40
#define PIT_CHANNEL_1_DATA_PORT 0x41
#define PIT_CHANNEL_2_DATA_PORT 0x42
#define PIT_CMD_REGISTER        0x43
	// Fields for the command register that we will use
	// Specifies that we are setting the mode for channel 0
	#define PIT_CMD_CHANNEL_0 0x0
	// Specifies that both the low and high byte of the counter value will be read / written (sequentially)
	#define PIT_CMD_ACCESS_MODE_LO_HI (0x3 << 4)
	// Mode which specifies that we will get interrupts at a constant rate
	#define PIT_CMD_MODE_2 (0x2 << 1)
	// Specifies that we will be using binary as opposed to BCD
	#define PIT_CMD_BINARY_MODE 0x0

// Frequency at which the PIT generates interrupts
#define PIT_FREQUENCY 69
// The base frequency of the PIT in Hz
#define PIT_BASE_FREQUENCY 1193182
// The reload value based off of the desired frequency and the base frequency
#define PIT_RELOAD_VALUE (PIT_BASE_FREQUENCY / PIT_FREQUENCY)

// Initializes the Programmable Interval Timer to generate interrupts at a frequency of very close to 69 Hz
void init_pit();

// Begins callbacks to scheduler_interrupt_handler at a rate of 69 Hz
void enable_scheduling();

// Handler for the timer interrupt
void timer_handler();

// Adds a callback to the list of callbacks that will be called at a periodic interval, returning
//  an ID that will be used to deregister the callback
// Callbacks must run quickly!
uint32_t register_periodic_callback(int interval, void (*callback)(double));

// Unregisters a previously registered callback
void unregister_periodic_callback(uint32_t id);

// Global time from startup in seconds
extern double sys_time;

#endif
