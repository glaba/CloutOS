#define ASM     1

#include "system_call_linkage.h"

.text

.extern is_userspace
.globl system_call_linkage

.align 4

# Function: system_call_linkage
# Description: linkage between IDT and system_call_handler
# Inputs: none
# Outputs: the return value of the relevant system call in eax
# Registers: saves all registers except eax, which holds the return value
system_call_linkage:
	sti                               # Enable interrupts, since many system calls rely on interrupts

	# Save all registers but eax, which will have the return value written into it
	# ***IMPORTANT*** If this list of registers is changed, process_halt must also be modified
    pushl %esi
    pushl %edi
    pushl %ebp
	pushl %edx                  	  # Push 1st parameter
	pushl %ecx						  # Push 2nd parameter
	pushl %ebx						  # Push 3rd parameter

	movl $0, in_userspace

	addl $-1, %eax					  # Subtract 1 for 0 based index
    cmpl $10, %eax                    # check eax values
	jae eax_error                	  # if eax - 1 >= 10, go to eax_error
	call *sys_call_table(, %eax, 4)	  # Jumps to correct system call
    jmp return_normal

eax_error:
	movl $-1, %eax                    # When system call number is wrong return -1

return_normal:
	movl $1, in_userspace

	# Pop all the registers that were saved except for eax, since that holds the return value
    popl %ebx
    popl %ecx
    popl %edx
    popl %ebp
    popl %edi
    popl %esi

	iret

sys_call_table:
	.long halt, execute, read, write, open, close, getargs, vidmap, set_handler, sigreturn
