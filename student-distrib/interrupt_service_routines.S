#define ASM     1
#include "interrupt_service_routines.h"

.text

.globl keyboard_linkage
.globl rtc_linkage
.globl

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
	jmp *jump_table(,%eax,10)


jump_table:
	.long system_call_halt, system_call_execute, system_call_read, system_call_write,
	system_call_open, system_call_close, system_call_getargs, system_call_vidmap,
	system_call_set_handler, system_call_sigreturn

system_call_halt:
	pushal                      # Push all general purpose registers in 32 bit form
	pushl %ebx
	call halt
	movl 4(%esp),%esp
	popal                       # Pop general purpose registers
	cmpl -1, %eax
	je err
	sti                         # Enable interrupts due to interrupt gate
	iret

system_call_execute:
	pushal                      # Push all general purpose registers in 32 bit form
	pushl %ebx
	call execute
	movl 4(%esp),%esp
	popal                       # Pop general purpose registers
	cmpl -1, %eax
	je err
	sti                         # Enable interrupts due to interrupt gate
	iret

system_call_read:
	pushal                      # Push all general purpose registers in 32 bit form
	pushl %edx
	pushl %ecx
	pushl %ebx
	call read
	movl 12(%esp),%esp
	popal                       # Pop general purpose registers
	cmpl -1, %eax
	je err
	sti                         # Enable interrupts due to interrupt gate
	iret

system_call_write:
	pushal                      # Push all general purpose registers in 32 bit form
	pushl %edx
	pushl %ecx
	pushl %ebx
	call read
	movl 12(%esp),%esp
	popal                       # Pop general purpose registers
	cmpl -1, %eax
	je err
	sti                         # Enable interrupts due to interrupt gate
	iret

system_call_open:
	pushal                      # Push all general purpose registers in 32 bit form
	pushl %ebx
	call read
	movl 4(%esp),%esp
	popal                       # Pop general purpose registers
	cmpl -1, %eax
	je err
	sti                         # Enable interrupts due to interrupt gate
	iret

system_call_close:
	pushal                      # Push all general purpose registers in 32 bit form
	pushl %ebx
	call close
	movl 4(%esp),%esp
	popal                       # Pop general purpose registers
	cmpl -1, %eax
	je err
	sti                         # Enable interrupts due to interrupt gate
	iret

system_call_getargs:
	pushal                      # Push all general purpose registers in 32 bit form
	pushl %ecx
	pushl %ebx
	call getargs
	movl 8(%esp),%esp
	popal                       # Pop general purpose registers
	cmpl -1, %eax
	je err
	sti                         # Enable interrupts due to interrupt gate
	iret

system_call_vidmap:
	pushal                      # Push all general purpose registers in 32 bit form
	pushl %ebx
	call vidmap
	movl 4(%esp),%esp
	popal                       # Pop general purpose registers
	cmpl -1, %eax
	je err
	sti                         # Enable interrupts due to interrupt gate
	iret

system_call_set_handler:
	pushal                      # Push all general purpose registers in 32 bit form
	pushl %ecx
	pushl %ebx
	call set_handler
	movl 8(%esp),%esp
	popal                       # Pop general purpose registers
	cmpl -1, %eax
	je err
	sti                         # Enable interrupts due to interrupt gate
	iret

system_call_sigreturn:
	pushal                      # Push all general purpose registers in 32 bit form
	call sigreturn
	popal                       # Pop general purpose registers
	cmpl -1, %eax
	je err
	sti                         # Enable interrupts due to interrupt gate
	iret

err:
	# Fixes error
