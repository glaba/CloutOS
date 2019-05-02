#define ASM     1

#include "interrupt_service_routines.h"

.text

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

# The stack pointer in the beginning of the timer linkage
.global timer_linkage_esp;
timer_linkage_esp:
	.long 0

/*
 * Use a macro to generate the all the linkages
 *
 * INPUTS: none
 * OUTPUTS: none
 * REGISTERS: saves and restores ebp, esp, eax, ebx, ecx, edx, edi, esi
 */
#define GEN_LINKAGE(name, handler) \
.global name; \
name: \
	common_interrupt_enter \
	call handler; \
	common_interrupt_exit \
	iret

GEN_LINKAGE(keyboard_linkage, keyboard_handler)
GEN_LINKAGE(rtc_linkage, rtc_handler)
GEN_LINKAGE(pci_linkage, pci_irq_handler)

# The timer has a special linkage
.global timer_linkage;
timer_linkage:
	# Copy the value of ESP excluding the IRET context into a special location
	addl $20, %esp
	movl %esp, timer_linkage_esp
	addl $-20, %esp
	# Now, do the regular linkage
	common_interrupt_enter
	call timer_handler
	common_interrupt_exit
	iret
