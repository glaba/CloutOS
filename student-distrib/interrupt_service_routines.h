#ifndef _INTERRUPT_SERVICE_ROUTINES_H
#define _INTERRUPT_SERVICE_ROUTINES_H

#ifndef ASM

#include "types.h"
#include "keyboard.h"
#include "rtc.h"
#include "system_calls.h"
#include "pci.h"
#include "pit.h"
#include "lib.h"
#include "mouse.h"

/* Linkage for keyboard interrupt handler */
extern void keyboard_linkage();

/* Linkage for RTC interrupt handler */
extern void rtc_linkage();

/* Linkage for mouse interrupt handler */
extern void mouse_linkage();

/* Linkage for system call handler */
extern void system_call_linkage();

/* Linkage for all PCI interrupt handlers */
extern void pci_linkage();

/* Linkage for the timer interrupt */
extern void timer_linkage();

// The value of ESP at the beginning of the timer linkage
extern uint32_t timer_linkage_esp;

#endif
#endif
