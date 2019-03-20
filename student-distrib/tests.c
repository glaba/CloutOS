#include "tests.h"
#include "x86_desc.h"
#include "lib.h"
#include "init_idt.h"
#include "exception_handlers.h"
#include "file_system.h"

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
	ptr =  (unsigned long) exception_handlers[DIVIDE_ZERO_E];
	ptr1 = (unsigned long) exception_handlers[NMINTERRUPT_E];
	ptr2 = (unsigned long) exception_handlers[BREAKPOINT_E];
	ptr3 = (unsigned long) exception_handlers[OVERFLOW_E];
	ptr4 = (unsigned long) exception_handlers[INVALID_OPCODE_E];

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

/*
 * Attempts to write to all valid paged memory, which should not cause an error
 *
 * INPUTS: None
 * RETURN VALUE: returns 1 if all regions that should be paged are paged and accessible for reading / writing
 * SIDE EFFECTS: none, if paging is correctly setup
 */
int paging_test_valid_regions() {
	TEST_HEADER;

	volatile unsigned char* ptr;
	volatile unsigned char value;

	printf("   Starting paging test...\n");

	// Attempt to access read and write every byte of memory that should be paged
	// First, access all bytes of video memory
	for (ptr = (unsigned char*)VIDEO; ptr < (unsigned char*)(VIDEO + VIDEO_SIZE); ptr++) {
		value = *ptr;
		*ptr = value;
	}

	printf("   Successfully performed read/write to all bytes of video memory.\n");

	// Then, access all bytes of kernel memory
	for (ptr = (unsigned char*)0x400000; ptr < (unsigned char*)0x800000; ptr++) {
		value = *ptr;
		*ptr = value;
	}

	printf("   Successfully performed read/write to all bytes of kernel memory.\n");

	return 1;
}

/*
 * Attempts to write to a region of memory that should not be paged, which should cause a page fault
 *
 * INPUTS: none
 * OUTPUTS: none
 * SIDE EFFECTS: if paging is correctly set up, it should cause a page fault
 */
void paging_test_invalid_region() {
	volatile unsigned char* ptr;
	volatile unsigned char value;

	printf("   Attempting to dereference unpaged pointer...\n");
	printf("   Should result in page fault...\n");

	ptr = NULL;
	value = *ptr;
}

/*
 * divide_by_zero_test
 * Attempts to divide by zero and should cause an exception
 *
 * INPUTS: None
 * OUTPUTS: None
 * SIDE EFFECTS: causes a divide by zero exception
 */
void divide_by_zero_test() {
	int num;
	int zero;
	int result;
	num = 10;
	zero = 0;
	result = num / zero;
}

/* Checkpoint 2 tests */
int test_fs() {
	TEST_HEADER;
	 dentry_t dentry;
	 dentry_t dentry2;
	 uint8_t buf_text[250];
	 //uint8_t buf_non_text[500];
	 uint8_t buf_large_text[6000];
	 // uint8_t buf_dir[120];
	 uint32_t i;
	 uint32_t buf_len;
	 /* test read directory */

	 printf("TESTING READ DIRECTORY\n");
	 buf_len = 1;
	 while(buf_len != 0){
		 buf_len = dir_read(buf_text);
		 if(buf_len == 0) break;
		 printf("file name: ");
		 for(i=0; i< buf_len; i++){
			 if(i == 32) break;
		   putc(buf_text[i]);
		 }
		 printf(" bytes read: %d", buf_len);
		 printf("\n");
	 }

	 i = 0;
	 while(i < 429496729) i++;

	 /* test read by name */
	 printf("TESTING READ BY NAME\n");
	 buf_len = 1;
	 while(buf_len != 0){
		 buf_len = dir_read(buf_text);
		 if(buf_len == 0) break;
		 printf("read by name ret val: %d\n", read_dentry_by_name(buf_text ,&dentry));
		 printf("fname: %s\n", dentry.filename);
		 printf("ftype: %d\n", dentry.filetype);
		 printf("inode: %d\n", dentry.inode);

		 i = 0;
		 while(i < 429496729) i++;
	 }

	 printf("TESTING READ BY INDEX\n");
	 	int j = 0;
 		for(j = 0; j < 17; j++){
 			read_dentry_by_index(j, &dentry2);
 			printf("index: %d\n", j);
 			printf("fname: %s\n", dentry2.filename);
 			printf("ftype: %d\n", dentry2.filetype);
 	 		printf("inode: %d\n", dentry2.inode);

 			i = 0;
 			while(i < 429496729) i++;
 		}

	 /* test reading large file*/
	printf("TESTING READ LARGE FILE\n");
	buf_len = read_data(44,0,buf_large_text,6000);
	printf("Number bytes read: %d\n", buf_len);
	printf("Since the file is too large,\nwe print the first and last 400 bytes in the file.\n");
	printf("\n");
	printf("First 400 bytes:\n");
	//for (i=0; i<bytes_read; i++)
	for (i=0; i<SIZE_THREAD/2; i++){
		printf("%c", buf_large_text[i]);
	}
	printf("\nLast 400 bytes:\n");
	for (i=buf_len-SIZE_THREAD/2; i<buf_len; i++){
		printf("%c", buf_large_text[i]);
	}
	printf("verylargetextwithverylongname.txt\n");

	i = 0;
	while(i < 429496729) i++;

	 read_test_text((uint8_t*)"frame0.txt");
	 	i = 0;
 		while(i < 429496729) i++;
		clear();
		read_test_text((uint8_t*)"frame1.txt");
		 i = 0;
		 while(i < 429496729) i++;
		 clear();
	 	read_test_exe((uint8_t*)"cat");
		i = 0;
		while(i < 429496729) i++;
		clear();
		read_test_exe((uint8_t*)"counter");
		i = 0;
		while(i < 429496729) i++;
		clear();
		read_test_exe((uint8_t*)"grep");
		i = 0;
		while(i < 429496729) i++;
		clear();
		read_test_exe((uint8_t*)"hello");
		i = 0;
		while(i < 429496729) i++;
		clear();
		read_test_exe((uint8_t*)"ls");
		i = 0;
		while(i < 429496729) i++;
		clear();
		read_test_exe((uint8_t*)"pingpong");
		i = 0;
		while(i < 429496729) i++;
		clear();
		read_test_exe((uint8_t*)"shell");
		i = 0;
		while(i < 429496729) i++;
		clear();
		read_test_exe((uint8_t*)"sigtest");
		i = 0;
		while(i < 429496729) i++;
		clear();
		read_test_exe((uint8_t*)"syserr");
		i = 0;
		while(i < 429496729) i++;
		clear();
		read_test_exe((uint8_t*)"testprint");
	return 1;
}
/* Checkpoint 3 tests */
/* Checkpoint 4 tests */
/* Checkpoint 5 tests */


/* Test suite entry point */
void launch_tests(){
	TEST_OUTPUT("idt_test", idt_test());
	TEST_OUTPUT("idt_test_extensive", idt_test_extensive());
	TEST_OUTPUT("paging_test_valid_regions", paging_test_valid_regions());
	TEST_OUTPUT("file_system_test", test_fs());

	// divide_by_zero_test();
	// paging_test_invalid_region();
}
