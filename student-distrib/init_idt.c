#include "x86_desc.h"
#include "lib.h"
#include "i8259.h"
#include "init_idt.h"

#define END_OF_EXCEPTIONS 32
#define SYSTEM_CALL_VECTOR 0x80

void initialize_idt(){
    int idt_idx;
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

}

void DIVIDE_ZERO_E(){
    printf("DIVIDE BY ZERO EXCEPTION\n");
    while(1){}
}
void DEBUG_E(){
    printf("DEBUG EXCEPTION\n");
    while(1){}
}
void NMINTERRUPT_E(){
    printf("NON MASKABLE INTERRUPT EXCEPTION\n");
    while(1){}
}
void BREAKPOINT_E(){
    printf("BREAKPOINT EXCEPTION\n");
    while(1){}
}
void OVERFLOW_E(){
    printf("OVERFLOW EXCEPTION\n");
    while(1){}
}
void BOUND_RANGE_EXCEEDED_E(){
    printf("BOUND RANGE EXCEEDED EXCEPTION\n");
    while(1){}
}
void INVALID_OPCODE_E(){
    printf("INVALID OPCODE EXCEPTION\n");
    while(1){}
}
void DEVICE_NA_E(){
    printf("DEVICE NOT AVAILABLE EXCEPTION\n");
    while(1){}
}
void DOUBLE_FAULT(){
    printf("DOUBLE FAULT EXCEPTION\n");
    while(1){}
}
void COPROCESSOR_SEGMENT_OVERRUN_E(){
    printf("COPROCESSOR SEGMENT EXCEPTION\n");
    while(1){}
}
void INVALID_TSS_E(){
    printf("INVALID TSS EXCEPTION\n");
    while(1){}
}
void SEGMENT_NP_E(){
    printf("SEGMENT NOT PRESENT EXCEPTION\n");
    while(1){}
}
void STACK_SEGMENT_FAULT_E(){
    printf("STACK SEGMENT FAULT EXCEPTION\n");
    while(1){}
}
void GENERAL_PROTECTION_E(){
    printf("GENERAL PROTECTION EXCEPTION\n");
    while(1){}
}
void PAGE_FAULT_E(){
    printf("PAGE FAULT EXCEPTION\n");
    while(1){}
}
void FLOATING_POINT_ERROR_E(){
    printf("FLOATING POINT ERROR EXCEPTION\n");
    while(1){}
}
void ALIGNMENT_CHECK_E(){
    printf("ALIGNMENT CHE EXCEPTION\n");
    while(1){}
}
void MACHINE_CHECK_E(){
    printf("MACHINE CHECK EXCEPTION\n");
    while(1){}
}
void FLOATING_POINT_EXCEPTION_E(){
    printf("SIMD FLOATING POINT EXCEPTION\n");
    while(1){}
}



