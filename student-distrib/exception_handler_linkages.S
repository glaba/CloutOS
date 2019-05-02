#define ASM     1

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

.globl divide_zero_linkage
divide_zero_linkage: 
	common_interrupt_enter
	call divide_zero_handler
	common_interrupt_exit
	iret

.globl debug_linkage
debug_linkage: 
	common_interrupt_enter
	call debug_handler
	common_interrupt_exit
	iret

.globl nminterrupt_linkage
nminterrupt_linkage: 
	common_interrupt_enter
	call nminterrupt_handler
	common_interrupt_exit
	iret

.globl breakpoint_linkage
breakpoint_linkage: 
	common_interrupt_enter
	call breakpoint_handler
	common_interrupt_exit
	iret

.globl overflow_linkage
overflow_linkage: 
	common_interrupt_enter
	call overflow_handler
	common_interrupt_exit
	iret

.globl bound_range_exceeded_linkage
bound_range_exceeded_linkage: 
	common_interrupt_enter
	call bound_range_exceeded_handler
	common_interrupt_exit
	iret

.globl invalid_opcode_linkage
invalid_opcode_linkage: 
	common_interrupt_enter
	call invalid_opcode_handler
	common_interrupt_exit
	iret

.globl device_na_linkage
device_na_linkage: 
	common_interrupt_enter
	call device_na_handler
	common_interrupt_exit
	iret

.globl double_fault_linkage
double_fault_linkage:
	add $4, %esp # Throw away the error code 
	common_interrupt_enter
	call double_fault_handler
	common_interrupt_exit
	iret

.globl coprocessor_segment_overrun_linkage
coprocessor_segment_overrun_linkage: 
	common_interrupt_enter
	call coprocessor_segment_overrun_handler
	common_interrupt_exit
	iret

.globl invalid_tss_linkage
invalid_tss_linkage: 
	add $4, %esp # Throw away the error code 
	common_interrupt_enter
	call invalid_tss_handler
	common_interrupt_exit
	iret

.globl segment_np_linkage
segment_np_linkage: 
	add $4, %esp # Throw away the error code 
	common_interrupt_enter
	call segment_np_handler
	common_interrupt_exit
	iret

.globl stack_segment_fault_linkage
stack_segment_fault_linkage: 
	add $4, %esp # Throw away the error code 
	common_interrupt_enter
	call stack_segment_fault_handler
	common_interrupt_exit
	iret

.globl general_protection_linkage
general_protection_linkage: 
	add $4, %esp # Throw away the error code 
	common_interrupt_enter
	call general_protection_handler
	common_interrupt_exit
	iret

.globl page_fault_linkage
page_fault_linkage: 
	add $4, %esp # Throw away the error code 
	common_interrupt_enter
	call page_fault_handler
	common_interrupt_exit
	iret

.globl floating_point_error_linkage
floating_point_error_linkage: 
	common_interrupt_enter
	call floating_point_error_handler
	common_interrupt_exit
	iret

.globl alignment_check_linkage
alignment_check_linkage: 
	add $4, %esp # Throw away the error code 
	common_interrupt_enter
	call alignment_check_handler
	common_interrupt_exit
	iret

.globl machine_check_linkage
machine_check_linkage: 
	common_interrupt_enter
	call machine_check_handler
	common_interrupt_exit
	iret

.globl floating_point_exception_linkage
floating_point_exception_linkage: 
	common_interrupt_enter
	call floating_point_exception_handler
	common_interrupt_exit
	iret

