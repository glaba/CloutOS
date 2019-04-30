#ifndef _SIGNALS_H
#define _SIGNALS_H

// The total number of signals in the OS
#define NUM_SIGNALS 5
// The indices of the signals
	#define SIGNAL_DIV_ZERO  0
	#define SIGNAL_SEGFAULT  1
	#define SIGNAL_INTERRUPT 2
	#define SIGNAL_ALARM     3
	#define SIGNAL_IO        4

typedef void (*signal_handler)(void);

#endif
