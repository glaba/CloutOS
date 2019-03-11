#include "tests.h"
#include "x86_desc.h"
#include "lib.h"
#include "init_idt.h"
#include "exception_handlers.h"


#define PASS 1
#define FAIL 0


/* format these macros as you see fit */
#define TEST_HEADER 	\
	printf("[TEST %s] Running %s at %s:%d\n", __FUNCTION__, __FUNCTION__, __FILE__, __LINE__)
#define TEST_OUTPUT(name, result)	\
	printf("[TEST %s] Result = %s\n", name, (result) ? "PASS" : "FAIL");

static inline void assertion_failure(){
	/* Use exception #15 for assertions, otherwise
	   reserved by Intel */
	asm volatile("int $15");
}


/* Checkpoint 1 tests */

/* IDT Test - Example
 * 
 * Asserts that first 10 IDT entries are not NULL
 * Inputs: None
 * Outputs: PASS/FAIL
 * Side Effects: None
 * Coverage: Load IDT, IDT definition
 * Files: x86_desc.h/S
 */
int idt_test(){
	TEST_HEADER;

	int i;
	int result = PASS;
	for (i = 0; i < 10; ++i){
		if ((idt[i].offset_15_00 == NULL) && 
			(idt[i].offset_31_16 == NULL)){
			assertion_failure();
			result = FAIL;
		}
	}

	return result;
}

/*   idt_test_extensive();
 *   DESCRIPTION: Tests whether entries in IDT have the 
 * 				  correct function addresses encoded in them			  
 *   INPUTS: NONE
 *   RETURN VALUE: NONE
 *   SIDE EFFECTS: NONE */ 
int idt_test_extensive(){
	TEST_HEADER;

	int result = PASS;
	unsigned long ptr;
	unsigned long ptr1;
	unsigned long ptr2;
	unsigned long ptr3;
	unsigned long ptr4;

	/* Holds the addresses for various exception handlers */
	ptr = (unsigned long) DIVIDE_ZERO_E;
	ptr1 = (unsigned long) NMINTERRUPT_E;
	ptr2 = (unsigned long) BREAKPOINT_E;
	ptr3 = (unsigned long) OVERFLOW_E;
	ptr4 = (unsigned long) INVALID_OPCODE_E;

	/* Makes sure the IDT holds correct address to exception handler for DIVIDE ZERO EXCEPTION*/
	if (idt[0].offset_31_16 != ((ptr & 0xFFFF0000) >> 16) && idt[0].offset_15_00 != (ptr & 0x0000FFFF)){
			printf("%#x %#x:%#x", ptr, ptr & 0xFFFF0000, ptr & 0x0000FFFF);
			assertion_failure();
			result = FAIL;
	}

	/* Makes sure the IDT holds correct address to exception handler for NON MASKABLE INTERRUPT*/
	if (idt[2].offset_31_16 != ((ptr1 & 0xFFFF0000) >> 16) && idt[2].offset_15_00 != (ptr1 & 0x0000FFFF)){
			printf("%#x %#x:%#x", ptr1 & 0xFFFF0000, ptr1 & 0x0000FFFF);
			assertion_failure();
			result = FAIL;
	}

	/* Makes sure the IDT holds correct address to exception handler for BREAKPOINT EXCEPTION*/
	if (idt[3].offset_31_16 != ((ptr2 & 0xFFFF0000) >> 16) && idt[3].offset_15_00 != (ptr2 & 0x0000FFFF)){
			printf("%#x %#x:%#x", ptr2 & 0xFFFF0000, ptr2 & 0x0000FFFF);
			assertion_failure();
			result = FAIL;
	}

	/* Makes sure the IDT holds correct address to exception handler for OVERFLOW EXCEPTION*/
	if (idt[4].offset_31_16 != ((ptr3 & 0xFFFF0000) >> 16) && idt[4].offset_15_00 != (ptr3 & 0x0000FFFF)){
			printf("%#x %#x:%#x", ptr3 & 0xFFFF0000, ptr3 & 0x0000FFFF);
			assertion_failure();
			result = FAIL;
	}

	/* Makes sure the IDT holds correct address to exception handler for INVALID OPCODE EXCEPTION*/
	if (idt[6].offset_31_16 != ((ptr4 & 0xFFFF0000) >> 16) && idt[6].offset_15_00 != (ptr4 & 0x0000FFFF)){
			printf("%#x %#x:%#x", ptr4 & 0xFFFF0000, ptr4 & 0x0000FFFF);
			assertion_failure();
			result = FAIL;
	}
	return result;
}

/* Checkpoint 2 tests */
/* Checkpoint 3 tests */
/* Checkpoint 4 tests */
/* Checkpoint 5 tests */


/* Test suite entry point */
void launch_tests(){
	// clear();
	TEST_OUTPUT("idt_test", idt_test());
	TEST_OUTPUT("idt_test_extensive", idt_test_extensive());
	// assertion_failure();
	// launch your tests here
}
