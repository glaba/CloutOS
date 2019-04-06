#define ASM     1
#define SIZE_OF_SYSTEM_CALL_TABLE 10

#include "interrupt_service_routines.h"

.text

.globl keyboard_linkage
.globl rtc_linkage
.globl system_call_linkage

.align 4



// Arguments needed to move pointer


# Function: keyboard_linkage
# Description: linkage between IDT and keyboard_handler
# Inputs   : none
# Outputs  : none
# Registers: saves and restores ebp, eax, ebx, ecx, edx, edi, esi, fl
keyboard_linkage:
	pushal                      # Push all general purpose registers in 32 bit form
	call keyboard_handler
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
	call rtc_handler
	popal                       # Pop general purpose registers
	sti                         # Enable interrupts due to interrupt gate
	iret

# Function: system_call_linkage
# Description: linkage between IDT and system_call_handler
# Inputs: none
# OUtputs: none
# Registers: saves and restores ebp, eax, ebx, ecx, edx, edi, esi, fl
system_call_linkage:
	pushal                      # Push all general purpose registers in 32 bit form
	#check eax values
	cmpl $SIZE_OF_TABLE,%eax
	ja eax_error                # if eax > 10 and eax < 0, go to eax_error
	pushl %edx                  # Push 1st register
	pushl %ecx					# Push 2nd register
	pushl %ebx					# Push 3rd register
	call *sys_call_table(,%eax,4)
	sti                         # Enable interrupts due to interrupt gate
	iret


sys_call_table:
	.long halt, execute, read, write,
	open, close, getargs, vidmap,
	set_handler, sigreturn
