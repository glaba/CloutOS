#ifndef _INTERRUPT_SERVICE_ROUTINES_H
#define _INTERRUPT_SERVICE_ROUTINES_H

#ifndef ASM

#include "types.h"
#include "keyboard.h"
#include "rtc.h"

/* Linkage for keyboard interrupt handler */
extern void keyboard_linkage();

/* Linkage for RTC interrupt handler */
extern void rtc_linkage();

#endif
#endif
