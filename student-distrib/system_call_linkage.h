#ifndef _SYSTEM_CALL_LINKAGE_H
#define  _SYSTEM_CALL_LINKAGE_H

#ifndef ASM

#include "types.h"
#include "system_calls.h"


// Set to 1 if a userspace process is currently running, and 0 if the kernel is running
extern uint32_t in_userspace;

/* Linkage for system call handler */
extern void system_call_linkage();

#endif
#endif
