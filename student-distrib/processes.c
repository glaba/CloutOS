#include "processes.h"
#include "lib.h"
#include "file_system.h"
#include "paging.h"
#include "x86_desc.h"

// An array indicating which PIDs are currently in use by running programs
static uint8_t used_pids[MAX_NUM_PROCESSES];

/*
 * Returns an unused PID, which is also the index of the kernel stack that will be used
 * If none is found, it returns -1
 */
int32_t get_open_pid() {
	int i;
	for (i = 0; i < MAX_NUM_PROCESSES; i++) {
		if (!used_pids[i]) {
			used_pids[i] = 1;
			return i;
		}
	}
	return -1;
}

/*
 * Starts the process associated with the given shell command
 * INPUTS: command: a string whose first word (up to the first space) specifies the filename
 *                  of an executable to be loaded into memory and ran
 */
int32_t start_process(const char *command) {
	cli();

	// Get the filename of the executable from the command
	char name[MAX_FILENAME_LENGTH + 1];
	int i;
	for (i = 0; i < MAX_FILENAME_LENGTH && command[i] != '\0' && command[i] != ' '; i++)
		name[i] = command[i];
	name[i] = '\0';

	// Get the PID for this process
	uint32_t cur_pid = get_open_pid();

	// Get the memory address where the executable will be placed and the page that contains it
	void *program_page = (void*)(USER_PROGRAMS_START_ADDR + LARGE_PAGE_SIZE * cur_pid);
	void *virt_prog_page = (void*)EXECUTABLE_VIRT_PAGE_START;
	void *virt_prog_location = (void*)EXECUTABLE_VIRT_PAGE_START + EXECUTABLE_PAGE_OFFSET;

	// Page in the memory region where the executable will be located so that we can write the executable
	if (map_region(program_page, virt_prog_page, 1, PAGE_READ_WRITE | PAGE_USER_LEVEL) != 0) {
		sti();
		return -1;
	}

	// Load the executable into memory at the address corresponding to the PID
	if (fs_load(name, virt_prog_location) != 0) {
		sti();
		return -1;
	}

	// Check that the magic number is present	
	if (*(uint32_t*)virt_prog_location != ELF_MAGIC) {
		sti();
		return -1;
	}

	// Get the entrypoint of the executable (virtual address) from the program data
	void *entrypoint = *((void**)(EXECUTABLE_VIRT_PAGE_START + EXECUTABLE_PAGE_OFFSET + ENTRYPOINT_OFFSET));

	// Set the stack pointer for the new program to just point to the top of the program
	void *program_esp = virt_prog_page + LARGE_PAGE_SIZE - 1;

	// Set the TSS's SS0 and ESP0 fields
	// SS0 should point to the kernel's stack segment
	tss.ss0 = KERNEL_DS;
	// ESP0 should point to the 8KB kernel stack for this process, which collectively all begin
	//  below the bottom of the kernel stack (which ranges from 8MB-8KB to 8MB)
	tss.esp0 = KERNEL_START_ADDR + LARGE_PAGE_SIZE - KERNEL_STACK_SIZE * (1 + cur_pid);

	// Decrement esp0 for the pcb
	tss.esp0 -= sizeof(pcb_t);
	// Put the pcb on the stack
	pcb_t *pcb = (pcb_t*)tss.esp0;

	// Initialize the fields of the PCB
	for (i = 0; i < MAX_NUM_FILES; i++)
		pcb->files[i].in_use = 0;
	pcb->pid = cur_pid;

	asm volatile ("         \n\
		movl %k0, %%eax     \n\
		movw %%ax, %%ds     \n\
		movw %%ax, %%es     \n\
		movw %%ax, %%fs     \n\
		movw %%ax, %%gs     \n\
		pushl %k0           \n\
		pushl %k1           \n\
		pushfl              \n\
		orl $0x200, (%%esp) \n\
		pushl %k2           \n\
		pushl %k3           \n"
		:
		: "r"((USER_DS)), "r"((program_esp)), "r"((USER_CS)), "r"((entrypoint))
		: "eax", "esp" // It clobbers a lot more than this but let's just put this much there for consistency
	);

	asm volatile ("iret");

	return 0;
}
