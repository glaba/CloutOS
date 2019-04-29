#define ASM     1

#include "interrupt_service_routines.h"

.text

.data 
	SIZE_OF_SYSTEM_CALL_TABLE = 10

.globl keyboard_linkage
.globl rtc_linkage
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

# Function: pci_linkage
# Description: linkage between IDT and pci_irq_handler
# Inputs   : none
# Outputs  : none
# Registers: saves and restores ebp, eax, ebx, ecx, edx, edi, esi, fl
pci_linkage:
	pushal                      # Push all general purpose registers in 32 bit form
	pushfl                      # Push flags
	call pci_irq_handler
	popfl                       # Pop flags
	popal                       # Pop general purpose registers
	sti                         # Enable interrupts due to interrupt gate
	iret

# Function: timer_linkage
# Description: linkage between IDT and timer_handler
# Inputs   : none
# Outputs  : none
# Registers: saves and restores ebp, eax, ebx, ecx, edx, edi, esi, fl
timer_linkage:
	pushal                      # Push all general purpose registers in 32 bit form
	pushfl                      # Push flags
	call timer_handler
	popfl                       # Pop flags
	popal                       # Pop general purpose registers
	sti                         # Enable interrupts due to interrupt gate
	iret
