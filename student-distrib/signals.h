#ifndef _SIGNALS_H
#define _SIGNALS_H

#include "types.h"

// The total number of signals in the OS
#define NUM_SIGNALS 5
// The indices of the signals
	#define SIGNAL_DIV_ZERO  0
	#define SIGNAL_SEGFAULT  1
	#define SIGNAL_INTERRUPT 2
	#define SIGNAL_ALARM     3
	#define SIGNAL_IO        4

// The frequency of the alarm signal in Hz
#define SIGNAL_ALARM_FREQ 1

// Values that signal_status can take in the PCB
#define SIGNAL_OPEN 0 // There is no pending signal
#define SIGNAL_PENDING 1 // There is a pending signal waiting to be dispatched
#define SIGNAL_HANDLING 2 // The signal is currently being handled

typedef void (*signal_handler)(void);

// Initializes signal functionality
int32_t init_signals();
// Sends a signal of given number to the process of given PID with some associated data
int32_t send_signal(int32_t pid, int32_t signum, uint32_t data);
// Checks if there are any signals pending for the current process. If so, it performs the corresponding
//  action, which may include calling a signal handler if it exists
void handle_signals();
// Cleans up any modifications made to the userspace stack due to a signal handler being called
//  and restores the process state to how it was before the signal handler was called for the process
//  corresponding to the current kernel stack
void cleanup_signal();

#endif
