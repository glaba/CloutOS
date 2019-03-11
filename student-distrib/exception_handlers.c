#include "x86_desc.h"
#include "lib.h"
#include "i8259.h"
#include "exception_handlers.h"


/* void DIVIDE_ZERO_E();
 * Inputs: NONE
 * Return Value: none
 * Function: squash user level program and cause exception */
void DIVIDE_ZERO_E(){
    cli();
    printf("DIVIDE BY ZERO EXCEPTION\n");
    sti();
    while(1){}
}

/* void DEBUG_E();
 * Inputs: NONE
 * Return Value: none
 * Function: squash user level program and cause exception */
void DEBUG_E(){
    cli();
    printf("DEBUG EXCEPTION\n");
    sti();
    while(1){}
}

/* void NMINTERRUPT_E();
 * Inputs: NONE
 * Return Value: none
 * Function: squash user level program and cause exception */
void NMINTERRUPT_E(){
    cli();
    printf("NON MASKABLE INTERRUPT EXCEPTION\n");
    sti();
    while(1){}
}

/* void BREAKPOINT_E();
 * Inputs: NONE
 * Return Value: none
 * Function: squash user level program and cause exception */
void BREAKPOINT_E(){
    cli();
    printf("BREAKPOINT EXCEPTION\n");
    sti();
    while(1){}
}

/* void OVERFLOW_E();
 * Inputs: NONE
 * Return Value: none
 * Function: squash user level program and cause exception */
void OVERFLOW_E(){
    cli();
    printf("OVERFLOW EXCEPTION\n");
    sti();
    while(1){}
}

/* void BOUND_RANGE_EXCEEDED_E();
 * Inputs: NONE
 * Return Value: none
 * Function: squash user level program and cause exception */
void BOUND_RANGE_EXCEEDED_E(){
    cli();
    printf("BOUND RANGE EXCEEDED EXCEPTION\n");
    sti();
    while(1){}
}

/* void INVALID_OPCODE_E();
 * Inputs: NONE
 * Return Value: none
 * Function: squash user level program and cause exception */
void INVALID_OPCODE_E(){
    cli();
    printf("INVALID OPCODE EXCEPTION\n");
    sti();
    while(1){}
}

/* void DEVICE_NA_E();
 * Inputs: NONE
 * Return Value: none
 * Function: squash user level program and cause exception */
void DEVICE_NA_E(){
    cli();
    printf("DEVICE NOT AVAILABLE EXCEPTION\n");
    sti();
    while(1){}
}

/* void DOUBLE_FAULT();
 * Inputs: NONE
 * Return Value: none
 * Function: squash user level program and cause exception */
void DOUBLE_FAULT(){
    cli();
    printf("DOUBLE FAULT EXCEPTION\n");
    sti();
    while(1){}
}

/* void COPROCESSOR_SEGMENT_OVERRUN_E();
 * Inputs: NONE
 * Return Value: none
 * Function: squash user level program and cause exception */
void COPROCESSOR_SEGMENT_OVERRUN_E(){
    cli();
    printf("COPROCESSOR SEGMENT EXCEPTION\n");
    sti();
    while(1){}
}

/* void INVALID_TSS_E();
 * Inputs: NONE
 * Return Value: none
 * Function: squash user level program and cause exception */
void INVALID_TSS_E(){
    cli();
    printf("INVALID TSS EXCEPTION\n");
    sti();
    while(1){}
}

/* void SEGMENT_NP_E();
 * Inputs: NONE
 * Return Value: none
 * Function: squash user level program and cause exception */
void SEGMENT_NP_E(){
    cli();
    printf("SEGMENT NOT PRESENT EXCEPTION\n");
    sti();
    while(1){}
}

/* void STACK_SEGMENT_FAULT_E();
 * Inputs: NONE
 * Return Value: none
 * Function: squash user level program and cause exception */
void STACK_SEGMENT_FAULT_E(){
    cli();
    printf("STACK SEGMENT FAULT EXCEPTION\n");
    sti();
    while(1){}
}

/* void GENERAL_PROTECTION_E();
 * Inputs: NONE
 * Return Value: none
 * Function: squash user level program and cause exception */
void GENERAL_PROTECTION_E(){
    cli();
    printf("GENERAL PROTECTION EXCEPTION\n");
    sti();
    while(1){}
}

/* void PAGE_FAULT_E();
 * Inputs: NONE
 * Return Value: none
 * Function: squash user level program and cause exception */
void PAGE_FAULT_E(){
    cli();
    printf("PAGE FAULT EXCEPTION\n");
    sti();
    while(1){}
}

/* void FLOATING_POINT_ERROR_E();
 * Inputs: NONE
 * Return Value: none
 * Function: squash user level program and cause exception */
void FLOATING_POINT_ERROR_E(){
    cli();
    printf("FLOATING POINT ERROR EXCEPTION\n");
    sti();
    while(1){}
}

/* void ALIGNMENT_CHECK_E();
 * Inputs: NONE
 * Return Value: none
 * Function: squash user level program and cause exception */
void ALIGNMENT_CHECK_E(){
    cli();
    printf("ALIGNMENT CHE EXCEPTION\n");
    sti();
    while(1){}
}

/* void MACHINE_CHECK_E();
 * Inputs: NONE
 * Return Value: none
 * Function: squash user level program and cause exception */
void MACHINE_CHECK_E(){
    cli();
    printf("MACHINE CHECK EXCEPTION\n");
    sti();
    while(1){}
}

/* void FLOATING_POINT_EXCEPTION_E();
 * Inputs: NONE
 * Return Value: none
 * Function: squash user level program and cause exception */
void FLOATING_POINT_EXCEPTION_E(){
    cli();
    printf("SIMD FLOATING POINT EXCEPTION\n");
    sti();
    while(1){}
}
