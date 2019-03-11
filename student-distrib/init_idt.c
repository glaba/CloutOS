#include "x86_desc.h"
#include "lib.h"
#include "i8259.h"
#include "init_idt.h"
#include "exception_handlers.h"
#include "interrupt_service_routines.h"

#define END_OF_EXCEPTIONS 32
#define SYSTEM_CALL_VECTOR 0x80
#define KEYBOARD_INTERRUPT 0x21
#define RTC_INTERRUPT 0x28

/*   initialize_idt();
 *   DESCRIPTION: Fills the IDT with entries and sets settings
 *           such as privilege level, and gate type			  
 *   INPUTS: NONE
 *   RETURN VALUE: NONE
 *   SIDE EFFECTS: Fills the IDT */ 
void initialize_idt(){
    int idt_idx;
    /* For the first 32 entries in IDT which are exceptions */
    for (idt_idx = 0; idt_idx < END_OF_EXCEPTIONS; idt_idx++){
        
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
    for (idt_idx = END_OF_EXCEPTIONS; idt_idx < NUM_VEC; idt_idx++){
        
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
    }
    /* Make sure system call handler is accessible from user space */
    idt[SYSTEM_CALL_VECTOR].dpl = 0x3;

    /* IDT entries for exceptions */
    SET_IDT_ENTRY(idt[0], DIVIDE_ZERO_E);
    SET_IDT_ENTRY(idt[1], DEBUG_E);
    SET_IDT_ENTRY(idt[2], NMINTERRUPT_E);
    SET_IDT_ENTRY(idt[3], BREAKPOINT_E);
    SET_IDT_ENTRY(idt[4], OVERFLOW_E);
    SET_IDT_ENTRY(idt[5], BOUND_RANGE_EXCEEDED_E);
    SET_IDT_ENTRY(idt[6], INVALID_OPCODE_E);
    SET_IDT_ENTRY(idt[7], DEVICE_NA_E);
    SET_IDT_ENTRY(idt[8], DOUBLE_FAULT);
    SET_IDT_ENTRY(idt[9], COPROCESSOR_SEGMENT_OVERRUN_E);
    SET_IDT_ENTRY(idt[10], INVALID_TSS_E);
    SET_IDT_ENTRY(idt[11], SEGMENT_NP_E);
    SET_IDT_ENTRY(idt[12], STACK_SEGMENT_FAULT_E);
    SET_IDT_ENTRY(idt[13], GENERAL_PROTECTION_E);
    SET_IDT_ENTRY(idt[14], PAGE_FAULT_E);
    SET_IDT_ENTRY(idt[16], FLOATING_POINT_ERROR_E);
    SET_IDT_ENTRY(idt[17], ALIGNMENT_CHECK_E);
    SET_IDT_ENTRY(idt[18], MACHINE_CHECK_E);
    SET_IDT_ENTRY(idt[19], FLOATING_POINT_EXCEPTION_E);

    /* IDT entries for keyboard and RTC */
    SET_IDT_ENTRY(idt[KEYBOARD_INTERRUPT], keyboard_linkage);
    SET_IDT_ENTRY(idt[RTC_INTERRUPT], rtc_linkage);
    
}




