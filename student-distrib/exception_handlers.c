#include "x86_desc.h"
#include "lib.h"
#include "i8259.h"
#include "exception_handlers.h"
#include "processes.h"

/*
 * This macro generates an exception handler
 *
 * INPUTS: handler_name: the name of the exception handler
 *         err: the string containing the name of the exception which will be printed
 *         line1_gen, line2_gen: function bodies which print out custom error messages (if desired)
 *              they return non-zero if something was printed, and optionally return 0 if nothing was printed
 */
#define GENERATE_EXCEPTION_HANDLER(handler_name, err, line1_gen, line2_gen) \
int line1_##handler_name(uint32_t err_code) {line1_gen return 0;}           \
int line2_##handler_name(uint32_t err_code) {line2_gen return 0;}           \
void handler_name(args) {                                                   \
	cli();                                                                  \
	check_userspace_exception();                                            \
	print_error(err, line1_##handler_name, line2_##handler_name, 0);        \
	while (1);                                                              \
	sti();                                                                  \
}

/*
 * This macro generates an exception handler for an exception that has an error code
 *
 * INPUTS: handler_name: the name of the exception handler
 *         err: the string containing the name of the exception which will be printed
 *         line1_gen, line2_gen: function bodies which print out custom error messages (if desired)
 *              they return non-zero if something was printed, and optionally return 0 if nothing was printed
 */
#define GENERATE_EXCEPTION_HANDLER_ERR(handler_name, err, line1_gen, line2_gen)   \
int line1_##handler_name(uint32_t err_code) {line1_gen return 0;}                 \
int line2_##handler_name(uint32_t err_code) {line2_gen return 0;}                 \
void handler_name(uint32_t err_code) {                                            \
	cli();                                                                        \
	check_userspace_exception();                                                  \
	print_error(err, line1_##handler_name, line2_##handler_name, err_code);       \
	while (1);                                                                    \
	sti();                                                                        \
}


/* An ASCII art skeleton that is 25 lines tall */
const char* skeleton = "\n\n\n\n\n\n\
    .-.    \n\
   (o.o)   \n\
    |=|    \n\
   __|__   \n\
 //.=|=.\\\\ \n\
// .=|=. \\\\\n\
\\\\ .=|=. //\n\
 \\\\(_=_)// \n\
  (:| |:)  \n\
   || ||   \n\
   () ()   \n\
   || ||   \n\
   || ||   \n\
  ==' '==\n\n\n\n\n";

/*
 * Checks if the current exception was raised by a userspace program, and calls halt on the
 *  program with a status of 256
 *
 * SIDE EFFECTS: returns execution to the parent process if the exception was raised by a
 *               userspace process
 */
void check_userspace_exception() {
	if (in_userspace) {
		process_halt(256);
	}
}

/*
 * Prints the error message for the provided exception name and two function pointers
 *  that generate 2 custom error lines
 *
 * INPUTS: err: the name of the exception
 *         line1, line2: functions which print custom error lines, and return non-zero if something was printed
 * SIDE EFFECTS: fills the screen with a "red screen of clout" (our error screen)
 */
void print_error(const char* err, int (*line1)(uint32_t), int (*line2)(uint32_t), uint32_t err_code) {
	unsigned int y = 7;

	set_color(V_RED, V_CYAN);
	clear();
	print_image(skeleton, 3, 0);

	set_cursor_location(17, y);
	printf("Greetings citizens of clout town. It is I, Flex Master Susan.");

	y += 2;
	set_cursor_location(17, y);
	printf("It is with great regret that I must inform you that");

	y += 1;
	set_cursor_location(19, y);
	printf("you have encountered a %s", err);

	y += 2;
	set_cursor_location(17, y);

	if (line1(err_code))
		y++;

	set_cursor_location(17, y);

	if (line2(err_code))
		y += 2;

	set_cursor_location(17, y);
	printf("That is all.");

	set_cursor_location(17, y + 2);
	printf("Regards,");

	set_cursor_location(17, y + 3);
	printf("Flex Master Susan");
}

GENERATE_EXCEPTION_HANDLER(divide_zero_e, "DIVIDE BY ZERO EXCEPTION", {}, {})
GENERATE_EXCEPTION_HANDLER(debug_e, "DEBUG EXCEPTION", {}, {})
GENERATE_EXCEPTION_HANDLER(nminterrupt_e, "NON MASKABLE INTERRUPT EXCEPTION", {}, {})
GENERATE_EXCEPTION_HANDLER(breakpoint_e, "BREAKPOINT EXCEPTION", {}, {})
GENERATE_EXCEPTION_HANDLER(overflow_e, "OVERFLOW EXCEPTION", {}, {})
GENERATE_EXCEPTION_HANDLER(bound_range_exceeded_e, "BOUND RANGE EXCEEDED EXCEPTION", {}, {})
GENERATE_EXCEPTION_HANDLER(invalid_opcode_e, "INVALID OPCODE EXCEPTION", {}, {})
GENERATE_EXCEPTION_HANDLER(device_na_e, "DEVICE NOT AVAILABLE EXCEPTION", {}, {})
GENERATE_EXCEPTION_HANDLER_ERR(double_fault, "DOUBLE FAULT EXCEPTION", {}, {})
GENERATE_EXCEPTION_HANDLER(coprocessor_segment_overrun_e, "COPROCESSOR SEGMENT EXCEPTION", {}, {})
GENERATE_EXCEPTION_HANDLER_ERR(invalid_tss_e, "INVALID TSS EXCEPTION", {}, {})
GENERATE_EXCEPTION_HANDLER_ERR(segment_np_e, "SEGMENT NOT PRESENT EXCEPTION", {}, {})
GENERATE_EXCEPTION_HANDLER_ERR(stack_segment_fault_e, "STACK SEGMENT FAULT EXCEPTION", {}, {})
GENERATE_EXCEPTION_HANDLER_ERR(general_protection_e, "GENERAL PROTECTION EXCEPTION", {
	return printf("Segment selector index that triggered fault: 0x%x", err_code);
}, {})
GENERATE_EXCEPTION_HANDLER_ERR(page_fault_e, "PAGE FAULT EXCEPTION", {
	uint32_t address;
    asm("movl %%cr2, %0": "=r" (address));
    return printf("Invalid memory access attempt at 0x%#x", address);
}, {
	return printf("caused page fault.");
})
GENERATE_EXCEPTION_HANDLER(floating_point_error_e, "FLOATING POINT ERROR EXCEPTION", {}, {})
GENERATE_EXCEPTION_HANDLER_ERR(alignment_check_e, "ALIGNMENT CHE EXCEPTION", {}, {})
GENERATE_EXCEPTION_HANDLER(machine_check_e, "MACHINE CHECK EXCEPTION", {}, {})
GENERATE_EXCEPTION_HANDLER(floating_point_exception_e, "SIMD FLOATING POINT EXCEPTION", {}, {})

/* Fill in exception handlers array with all exception handlers generated by the macro */
uint32_t exception_handlers[NUM_EXCEPTION_HANDLERS] = {
	(uint32_t)divide_zero_e,
	(uint32_t)debug_e,
	(uint32_t)nminterrupt_e,
	(uint32_t)breakpoint_e,
	(uint32_t)overflow_e,
	(uint32_t)bound_range_exceeded_e,
	(uint32_t)invalid_opcode_e,
	(uint32_t)device_na_e,
	(uint32_t)double_fault,
	(uint32_t)coprocessor_segment_overrun_e,
	(uint32_t)invalid_tss_e,
	(uint32_t)segment_np_e,
	(uint32_t)stack_segment_fault_e,
	(uint32_t)general_protection_e,
	(uint32_t)page_fault_e,
	(uint32_t)NULL,
	(uint32_t)floating_point_error_e,
	(uint32_t)alignment_check_e,
	(uint32_t)machine_check_e,
	(uint32_t)floating_point_exception_e
};
