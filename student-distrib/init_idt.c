#include "x86_desc.h"
#include "lib.h"
#include "i8259.h"
#include "init_idt.h"
#include "exception_handlers.h"
#include "interrupt_service_routines.h"
#include "irq_defs.h"

#define END_OF_EXCEPTIONS 32
#define SYSTEM_CALL_VECTOR 0x80
#define KEYBOARD_INTERRUPT (0x20 + KEYBOARD_IRQ)
#define RTC_INTERRUPT (0x20 + RTC_IRQ)
#define PCI_INTERRUPT (0x20 + PCI_IRQ)
#define MOUSE_INTERRUPT (0x20 + MOUSE_IRQ)
#define TIMER_INTERRUPT (0x20 + TIMER_IRQ)

/*   initialize_idt();
 *   DESCRIPTION: Fills the IDT with entries and sets settings
 *           such as privilege level, and gate type
 *   INPUTS: NONE
 *   RETURN VALUE: NONE
 *   SIDE EFFECTS: Fills the IDT */ 
void initialize_idt() {
	int idt_idx;

	/* For the first 32 entries in IDT which are exceptions */
	for (idt_idx = 0; idt_idx < END_OF_EXCEPTIONS; idt_idx++) {
		/* The following reserved bit settings set first 32 entries as TRAP gates */
		idt[idt_idx].reserved0 = 0x0;
		idt[idt_idx].reserved1 = 0x1;
		idt[idt_idx].reserved2 = 0x1;
		idt[idt_idx].reserved3 = 0x0;
		idt[idt_idx].reserved4 = 0x0;
		idt[idt_idx].seg_selector = KERNEL_CS;
		idt[idt_idx].size = 0x1;
		idt[idt_idx].present = 0x1;
		idt[idt_idx].dpl = 0x0;
	}

	/* For the other entries up to 255 for interrupts */
	for (idt_idx = END_OF_EXCEPTIONS; idt_idx < NUM_VEC; idt_idx++) {
		/* The following reserved bit settings set entries 32 to 255 as interrupt gates */
		idt[idt_idx].reserved0 = 0x0;
		idt[idt_idx].reserved1 = 0x1;
		idt[idt_idx].reserved2 = 0x1;
		idt[idt_idx].reserved3 = 0x1;
		idt[idt_idx].reserved4 = 0x0;
		idt[idt_idx].seg_selector = KERNEL_CS;
		idt[idt_idx].size = 0x1;
		idt[idt_idx].present = 0x1;
		idt[idt_idx].dpl = 0x0;
		
		if (idt_idx == SYSTEM_CALL_VECTOR) {
			/* The following reserved bit setting sets first 32 entries as TRAP gates 
			   which means that interrupts will not be disabled */
			idt[idt_idx].reserved3 = 0x0;
		
			/* Make sure system call handler is accessible from user space */
			idt[idt_idx].dpl = 0x3;
		}
	}

	/* IDT entries for exceptions */
	for (idt_idx = 0; idt_idx < NUM_EXCEPTION_HANDLERS; idt_idx++) {
		if (idt_idx == RESERVED_EXCEPTION_INDEX)
			continue;

		SET_IDT_ENTRY(idt[idt_idx], exception_handlers[idt_idx]);
	}

	/* IDT entries for interrupt handlers */
	SET_IDT_ENTRY(idt[KEYBOARD_INTERRUPT], keyboard_linkage);
	SET_IDT_ENTRY(idt[RTC_INTERRUPT], rtc_linkage);
	SET_IDT_ENTRY(idt[PCI_INTERRUPT], pci_linkage);
	SET_IDT_ENTRY(idt[MOUSE_INTERRUPT], mouse_linkage);
	SET_IDT_ENTRY(idt[TIMER_INTERRUPT], timer_linkage);

	// IDT entry for system calls
	SET_IDT_ENTRY(idt[SYSTEM_CALL_VECTOR], system_call_linkage);
}
