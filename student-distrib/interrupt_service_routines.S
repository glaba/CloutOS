#define ASM     1

#include "interrupt_service_routines.h"

.text

.data 
	SIZE_OF_SYSTEM_CALL_TABLE = 10

.globl keyboard_linkage
.globl rtc_linkage
.globl mouse_linkage
.globl system_call_linkage
.globl pci_linkage
.globl timer_linkage

.align 4

# Function: keyboard_linkage
# Description: linkage between IDT and keyboard_handler
# Inputs   : none
# Outputs  : none
# Registers: saves and restores ebp, eax, ebx, ecx, edx, edi, esi, fl
keyboard_linkage:
	pushal                      # Push all general purpose registers in 32 bit form
	call keyboard_handler
	popal                       # Pop general purpose registers
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
	iret

# Function: mouse_linkage
# Description: linkage between IDT and mouse_handler
# Inputs   : none
# Outputs  : none
# Registers: saves and restores ebp, eax, ebx, ecx, edx, edi, esi, fl
mouse_linkage:
	cli
	pushal                      # Push all general purpose registers in 32 bit form
	call mouse_handler
	popal                       # Pop general purpose registers
	sti
	iret

# Function: pci_linkage
# Description: linkage between IDT and pci_irq_handler
# Inputs   : none
# Outputs  : none
# Registers: saves and restores ebp, eax, ebx, ecx, edx, edi, esi, fl
pci_linkage:
	pushal                      # Push all general purpose registers in 32 bit form
	call pci_irq_handler
	popal                       # Pop general purpose registers
	iret

# Function: timer_linkage
# Description: linkage between IDT and timer_handler
# Inputs   : none
# Outputs  : none
# Registers: saves and restores ebp, eax, ebx, ecx, edx, edi, esi, fl
timer_linkage:
	pushal                      # Push all general purpose registers in 32 bit form
	call timer_handler
	popal                       # Pop general purpose registers
	iret
