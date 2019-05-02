#define ASM     1

#include "system_call_linkage.h"
#include "interrupt_service_routines.h"

.text

.globl system_call_linkage
.globl in_userspace

.align 4

#define common_interrupt_enter \
	pushw %fs; \
	pushw $0; \
	pushw %es; \
	pushw $0; \
	pushw %ds; \
	pushw $0; \
	pushl %eax; \
    pushl %ebp; \
    pushl %edi; \
    pushl %esi; \
	pushl %edx; \
	pushl %ecx; \
	pushl %ebx; \

#define common_interrupt_exit \
	popl %ebx; \
	popl %ecx; \
	popl %edx; \
    popl %esi; \
    popl %edi; \
    popl %ebp; \
	popl %eax; \
	add $2, %esp; \
	popw %ds; \
	add $2, %esp; \
	popw %es; \
	add $2, %esp; \
	popw %fs; \

// Set to 1 if a userspace process is currently running, and 0 if the kernel is running
in_userspace:
	.long 0

# Function: system_call_linkage
# Description: linkage between IDT and system_call_handler
# Inputs: none
# Outputs: the return value of the relevant system call in eax
# Registers: saves all registers except eax, which holds the return value
system_call_linkage:
	# Save all registers, including eax, although the stack entry will be replaced by the system call
	# common_interrupt_enter will place edx, ecx, and ebx on the top of the stack as the 3 parameters
	#  of the system call
	common_interrupt_enter

	movl $0, in_userspace

	pushl %eax    # Push it onto the stack for the next call 
	call sys_call
	addl $4, %esp # Pop off the extra argument pushed onto the stack

	movl $1, in_userspace

	# Pop all saved registers, which may have been modified
	common_interrupt_exit

	iret