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
	call handle_signals; \
	common_interrupt_exit \
	iret

GEN_LINKAGE(keyboard_linkage, keyboard_handler)
GEN_LINKAGE(rtc_linkage, rtc_handler)
GEN_LINKAGE(pci_linkage, pci_irq_handler)
GEN_LINKAGE(mouse_linkage, mouse_handler)
GEN_LINKAGE(timer_linkage, timer_handler)
