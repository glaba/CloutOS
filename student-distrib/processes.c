#include "processes.h"
#include "lib.h"
#include "file_system.h"
#include "paging.h"
#include "x86_desc.h"

// An array indicating which PIDs are currently in use by running programs
static uint8_t used_pids[MAX_NUM_PROCESSES];

static struct fops_t stdin_table  = {.open = NULL, .close = NULL,
	                                 .read = (int32_t (*)(int32_t, void*, int32_t))&terminal_read,
	                                 .write = NULL};
static struct fops_t stdout_table = {.open = NULL, .close = NULL,
	                                 .read = NULL,
	                                 .write = (int32_t (*)(int32_t, const void*, int32_t))&terminal_write};

/*
 * Sets the value of in_userspace to the given input
 */
void set_in_userspace(uint32_t value) {
	in_userspace = value & 0x1;
}

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
 * Get the current pcb from tss.esp0
 * INPUTS: N/A
 */
pcb_t* get_pcb() {
	// Initialize pcb pointer that will be moved
	void* addr_of_pcb;
	// Mask out lower 8 kB to get to prev 8kB
	addr_of_pcb = (void*)(tss.esp0 & KERNEL_STACK_BASE_BITMASK);
	// Add 8kB in order to round up to the base of the stack
	addr_of_pcb += KERNEL_STACK_SIZE;
	// Decrement to get to the start address of the pcb
	addr_of_pcb -= sizeof(pcb_t);
	// Put the pcb on the stack
	return (pcb_t*)addr_of_pcb;
}

/*
 * Halts the current process and returns the provided status code to the parent process
 * INPUTS: status: the status with which the program existed (256 for exception, [0-256) otherwise)
 * SIDE EFFECTS: it jumps to the kernel stack for the parent process and returns
 *               the correct status value from execute
 */
int32_t process_halt(uint16_t status) {
	// Get the PCB corresponding to this kernel stack
	pcb_t *pcb = get_pcb();

	// Mark the current PID as unused
	used_pids[pcb->pid] = 0;

	// Unmap video memory if it was mapped in
	if (pcb->vid_mem != NULL)
		unmap_video_mem_user(pcb->vid_mem);

	// If the parent PID is -1, that means that this is the parent shell and we should
	//  simply spawn a new shell
	if (pcb->parent_pid == -1)
		process_execute("shell", 0);

	// Otherwise, we have a normal process that has a parent
	// Compute the address of the page for the parent process
	uint32_t parent_page = USER_PROGRAMS_START_ADDR + LARGE_PAGE_SIZE * pcb->parent_pid;

	// Remap the virtual address at 128MB to point to the parent page
	unmap_region((void*)EXECUTABLE_VIRT_PAGE_START, 1);
	map_region((void*)parent_page, (void*)EXECUTABLE_VIRT_PAGE_START, 1, PAGE_READ_WRITE | PAGE_USER_LEVEL);

	// Compute the address of the top of the parent's kernel stack
	uint32_t esp = KERNEL_START_ADDR + LARGE_PAGE_SIZE - KERNEL_STACK_SIZE * (1 + pcb->parent_pid);

	// Set TSS.ESP0 to point to where the top of the stack will be, containing only the pcb
	tss.esp0 = esp - sizeof(pcb_t);

	// Push esp down to where it was when execute() was called
	// The stack will contain, in order:
	//  - the PCB
	//  - since it is a privilege switching interrupt: userspace DS segment selector, userspace esp
	//  - EFLAGS, userspace CS segment selector, userspace return address
	//  - 3 saved 32-bit registers
	//  - 3 32-bit parameters from registers edx, ecx, ebx
	//  - a 32-bit return address
	//  - possibly some other stuff that we don't care about
	// In total, one PCB and twelve 32-bit values (that we care about)
	// So we can decrement esp by the corresponding amount
	esp = esp - sizeof(pcb_t) - 12 * 4;
	// esp now points to the return address to the assembly linkage

	// We are switching back to userspace
	in_userspace = 1;

	// Set eax to the desired return value, set esp to the the esp of the parent process
	//  and return back to the assembly linkage
	asm volatile ("      \n\
		movl %k0, %%eax  \n\
		movl %k1, %%esp  \n\
		ret"
		:
		: "r"(((uint32_t)status)), "r"((esp))
		: "eax", "esp" // An entirely unsufficient and pointless list
	);

	// Placeholder to get gcc to shut up
	return 0;
}

/*
 * Starts the process associated with the given shell command
 * INPUTS: command: a string whose first word (up to the first space) specifies the filename
 *                  of an executable to be loaded into memory and ran
 *         has_parent: 1 if the newly created process has a parent, and 0 if not
 */
int32_t process_execute(const char *command, uint8_t has_parent) {
	cli();

	// Get the filename of the executable from the command
	char name[MAX_FILENAME_LENGTH + 1];
	int i;
	for (i = 0; i < MAX_FILENAME_LENGTH && command[i] != '\0' && command[i] != ' '; i++)
		name[i] = command[i];
	name[i] = '\0';

	//Save the current position into idx variable
	int idx = i;

	// Get the PID of the current parent process (if it exists)
	pcb_t *parent_pcb = get_pcb();
	int32_t parent_pid = has_parent ? parent_pcb->pid : -1;

	// Get the PID for this process
	int32_t cur_pid = get_open_pid();
	if (cur_pid < 0) {
		sti();
		return -1;
	}

	// Get the memory address where the executable will be placed and the page that contains it
	void *program_page = (void*)(USER_PROGRAMS_START_ADDR + LARGE_PAGE_SIZE * cur_pid);
	void *virt_prog_page = (void*)EXECUTABLE_VIRT_PAGE_START;
	void *virt_prog_location = (void*)EXECUTABLE_VIRT_PAGE_START + EXECUTABLE_PAGE_OFFSET;

	// Forcibly page in the memory region where the executable will be located so that we can write the executable
	unmap_region(virt_prog_page, 1);
	map_region(program_page, virt_prog_page, 1, PAGE_READ_WRITE | PAGE_USER_LEVEL);

	// Load the executable into memory at the address corresponding to the PID
	if (fs_load(name, virt_prog_location) != 0) {
		goto process_execute_fail;
	}

	// Check that the magic number is present
	if (*(uint32_t*)virt_prog_location != ELF_MAGIC) {
		goto process_execute_fail;
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
	pcb->parent_pid = parent_pid;
	pcb->vid_mem = NULL;
	// Set stdin and stdout as files 0 and 1 respectively
	pcb->files[STDIN].in_use = 1;
	pcb->files[STDIN].fd_table = &stdin_table;
	pcb->files[STDOUT].in_use = 1;
	pcb->files[STDOUT].fd_table = &stdout_table;

	/* Check the current position in command is a space, which would mean
	 * the rest of command would have the arguments
	 */
	if(command[idx] == ' ') {
		// Increment position to start from the first position of the arguments
		idx++;

		//Save where i started to help know where in pcb's arg array to store
		int start_of_arg = idx;
		// Get the arguments and store it into args array
		for (; idx < MAX_FILENAME_LENGTH && command[idx] != '\0'; idx++) {
			pcb->args[idx-start_of_arg] = command[idx];
		}
		pcb->args[idx-start_of_arg] = '\0';
	}

	// Else, there's no arguments

	// We are switching to userspace
	in_userspace = 1;

	// Push parameters onto stack for IRET into userspace
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

	// Jump into userspace
	asm volatile ("iret");

process_execute_fail:
	// Remap the parent's memory, assuming it was mapped to the parent process with pid parent_pid
	// Note that this doesn't make sense for the first process, which has no parent,
	//  so that call had better not fail
	unmap_region(virt_prog_page, 1);
	map_region((void*)(USER_PROGRAMS_START_ADDR + LARGE_PAGE_SIZE * parent_pid), virt_prog_page, 1,
		PAGE_READ_WRITE | PAGE_USER_LEVEL);

	// Set the PID as unused
	used_pids[cur_pid] = 0;

	// Restore interrupts and return -1
	sti();
	return -1;
}
