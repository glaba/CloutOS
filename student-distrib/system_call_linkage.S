#define ASM     1

#include "system_call_linkage.h"

.text

.globl system_call_linkage

.align 4

# Function: system_call_linkage
# Description: linkage between IDT and system_call_handler
# Inputs: none
# Outputs: none
# Registers: saves and restores ebp, eax, ebx, ecx, edx, edi, esi, fl
system_call_linkage:
    pushal
	pushl %edx                  			# Push 1st register
	pushl %ecx								# Push 2nd register
	pushl %ebx								# Push 3rd register
	
    cmpl $10, %eax                          # check eax values
	ja eax_error                			# if eax > 10 or eax < 0, go to eax_error
	cmpl $0, %eax                           # check eax == 0
	je eax_error
	addl $-1, %eax							# Subtract 1 for 0 based index
	call *sys_call_table(, %eax, 4)			# Jumps to correct system call
    jmp return_normal

eax_error:	
	movl $-1, %eax                          # When system call number is wrong return -1

return_normal:
    popl %ebx
    popl %ecx
    popl %edx
    popal
	
	iret


sys_call_table:
	.long halt, execute, read, write, open, close, getargs, vidmap, set_handler, sigreturn
