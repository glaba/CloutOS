#define ASM     1
#include "interrupt_service_routines.h"

.text

.globl keyboard_linkage
.globl rtc_linkage

.align 4

# Function: keyboard_linkage
# Description: linkage between IDT and keyboard_handler
# Inputs   : none
# Outputs  : none
# Registers: saves and restores ebp, eax, ebx, ecx, edx, edi, esi, fl
keyboard_linkage:
	pushal                      # Push all general purpose registers in 32 bit form
	pushfl                      # Push flags
	call keyboard_handler
	popfl                       # Pop flags
	popal                       # Pop general purpose registers
	sti                         # Enable interrupts due to interrupt gate
	iret

# Function: rtc_linkage
# Description: linkage between IDT and rtc_handler
# Inputs   : none
# Outputs  : none
# Registers: saves and restores ebp, eax, ebx, ecx, edx, edi, esi, fl
rtc_linkage:
	pushal                      # Push all general purpose registers in 32 bit form
	pushfl                      # Push flags
	call rtc_handler
	popfl                       # Pop flags
	popal                       # Pop general purpose registers
	sti                         # Enable interrupts due to interrupt gate
	iret
