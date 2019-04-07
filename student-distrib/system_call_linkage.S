#define ASM     1

#include "system_call_linkage.h"

.text

.data 
	SIZE_OF_SYSTEM_CALL_TABLE = 10

.globl system_call_linkage

.align 4

# Function: system_call_linkage
# Description: linkage between IDT and system_call_handler
# Inputs: none
# OUtputs: none
# Registers: saves and restores ebp, eax, ebx, ecx, edx, edi, esi, fl
system_call_linkage:
    pushl %ebp
	pushl %edi
	pushl %esi
	pushl %edx                  			# Push 1st register
	pushl %ecx								# Push 2nd register
	pushl %ebx								# Push 3rd register
	
    cmpl %eax, SIZE_OF_SYSTEM_CALL_TABLE	# check eax values
	ja eax_error                			# if eax > 10 and eax < 0, go to eax_error
	addl %eax, -1							# Subtract 1 for 0 based index
	call *sys_call_table(, %eax, 4)			# Jumps to correct system call
    jmp return_normal

eax_error:	
    xorl %eax, %eax                 
    addl %eax, -1                           # When system call number is wrong return -1

return_normal:
    popl %ebx
    popl %ecx
    popl %edx
    popl %esi
    popl %edi
    popl %ebp
	
	iret


sys_call_table:
	.long halt, execute, read, write, open, close, getargs, vidmap, set_handler, sigreturn
