#ifndef _INTERRUPT_SERVICE_ROUTINES_H
#define _INTERRUPT_SERVICE_ROUTINES_H

#ifndef ASM

#include "types.h"
#include "keyboard.h"
#include "rtc.h"
#include "system_calls.h"

/* Linkage for keyboard interrupt handler */
extern void keyboard_linkage();

/* Linkage for RTC interrupt handler */
extern void rtc_linkage();

// Linkage for all system calls
extern int32_t halt(uint8_t status);
extern int32_t execute(const uint8_t* command);
extern int32_t read(int32_t fd, void* buf, int32_t nbytes);
extern int32_t write(int32_t fd, const void* buf, int32_t nbytes);
extern int32_t open(const uint8_t* filename);
extern int32_t close(int32_t fd);
extern int32_t getargs(uint8_t* buf, int32_t nbytes);
extern int32_t vidmap(uint8_t** screen_start);
extern int32_t set_handler(int32_t signum, void* handler_address);
extern int32_t sigreturn(void);


#endif
#endif
