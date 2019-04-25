#include "processes.h"
#include "lib.h"
#include "file_system.h"
#include "paging.h"
#include "x86_desc.h"
#include "kheap.h"

// A dynamic array indicating which PIDs are currently in use by running programs
// Each index corresponds to a PID and contains a pointer to that process' PCB
typedef DYNAMIC_ARRAY(pcb_t, pcb_dyn_arr) pcb_dyn_arr;
pcb_dyn_arr pcbs;

static struct fops_t stdin_table  = {.open = NULL, .close = NULL,
	                                 .read = (int32_t (*)(int32_t, void*, int32_t))&terminal_read,
	                                 .write = NULL};
static struct fops_t stdout_table = {.open = NULL, .close = NULL,
	                                 .read = NULL,
	                                 .write = (int32_t (*)(int32_t, const void*, int32_t))&terminal_write};

/*
 * Initializes any supporting data structures for managing user level processes
 *
 * OUTPUTS: -1 on critical failure that should stop the OS (very extremely unlikely), 0 otherwise
 */
int init_processes() {
	DYN_ARR_INIT(pcb_t, pcbs);
	if (pcbs.data == NULL)
		return -1;
	return 0;
}

/*
 * Sets the value of in_userspace to the given input
 */
void set_in_userspace(uint32_t value) {
	in_userspace = value & 0x1;
}

/*
 * Returns an unused PID, but does not mark it used (marking it used is the caller's responsibility)
 * If none can be found, it returns -1
 *
 * SIDE EFFECTS: may resize pcbs if there are no unused PIDs
 */
int32_t get_open_pid() {
	int i;
	for (i = 0; i < pcbs.length; i++) {
		if (pcbs.data[i].pid < 0)
			return i;
	}

	// If we got this far, that means no PID was found
	// Add a new empty entry to the dynamic array
	pcb_t new_pcb;
	new_pcb.pid = -1;
	int pid = DYN_ARR_PUSH(pcb_t, pcbs, new_pcb);

	// Return the pid
	// If pushing to pcbs failed, it will be -1 which already means failure, so we need no more checks
	return pid;
}

/*
 * Gets the PCB corresponding to the given PID
 *
 * INPUTS: pid: a non-negative number indicating the PID of the process whose PCB we want
 * OUTPUTS: a pointer to the desired process' PCB (NULL if the process doesn't exist)
 */
pcb_t* get_pcb_from_pid(int32_t pid) {
	if (pid < 0 || pid >= pcbs.length)
		return NULL;

	return &pcbs.data[pid];
}

/*
 * Get the current pcb from tss.esp0
 * INPUTS: N/A
 */
pcb_t* get_pcb() {
	// Initialize a pointer to the PID which is stored at the base of the kernel stack
	void* addr_of_pid;
	// Mask out lower 8 kB to get to prev 8kB
	addr_of_pid = (void*)(tss.esp0 & KERNEL_STACK_BASE_BITMASK);
	// Add 8kB in order to round up to the base of the stack
	addr_of_pid += KERNEL_STACK_SIZE;
	// Decrement to get to the PID
	addr_of_pid -= sizeof(int32_t);
	// Return the PCB corresponding to the PID
	return get_pcb_from_pid(*(int32_t*)addr_of_pid);
}

/*
 * Checks if the given region of memory lies within the memory assigned to the process with the given PID 
 *
 * INPUTS: ptr: the base virtual address of the region of memory
 *         size: the size, in bytes of the region of memory
 *         pid: we are making sure the region lies within the memory assigned to the process with this PID
 * RETURNS: 0 if the region is valid and -1 otherwise
 */
int8_t is_userspace_region_valid(void *ptr, uint32_t size, int32_t pid) {
	if (((uint32_t)ptr < EXECUTABLE_VIRT_PAGE_START) || 
	    ((uint32_t)ptr + size >= EXECUTABLE_VIRT_PAGE_START + LARGE_PAGE_SIZE))
		return -1;

	return 0;
}

/*
 * Checks if the given string lies within the memory assigned to the process with the given PID
 *
 * INPUTS: ptr: the start address of the string
 *         pid: we are making sure the string lies within the memory assigned to the process with this PID
 * RETURNS: 0 if the string lies within the process' memory and -1 otherwise
 */
int8_t is_userspace_string_valid(void *ptr, int32_t pid) {
	// Check if the string is valid by iterating through it and checking each address for validity
	for (; *(uint8_t*)ptr != '\0'; ptr++) {
		// In either case, the current pointer is outside the valid range
		if (((uint32_t)ptr < EXECUTABLE_VIRT_PAGE_START) || 
		    ((uint32_t)ptr >= EXECUTABLE_VIRT_PAGE_START + LARGE_PAGE_SIZE))
			return -1;
	}

	return 0;
}

/*
 * Forcibly maps in the pages allocated to the process with given PID
 *
 * INPUTS: pid: the PID of the process whose pages we are paging in
 * OUTPUTS: -1 if the PID is invalid and 0 on success
 */
int32_t map_process(int32_t pid) {
	pcb_t *pcb = get_pcb_from_pid(pid);
	if (pcb == NULL)
		return -1;

	// Iterate through all the page mappings for this PID
	large_page_mapping_list_item *cur;
	for (cur = pcb->page_mappings_head; cur != NULL; cur = cur->next) {
		// Unmap the region first, since map_region checks if the region is already paged in
		unmap_region((void*)(cur->data.virt_index * LARGE_PAGE_SIZE), 1);
		// Map in the single 4MB region
		map_region((void*)(cur->data.phys_index * LARGE_PAGE_SIZE), 
			(void*)(cur->data.virt_index * LARGE_PAGE_SIZE), 1, PAGE_READ_WRITE | PAGE_USER_LEVEL);
	}

	return 0;
}

/*
 * Forcibly unmaps the pages allocated to the process with given PID
 *
 * INPUTS: pid: the PID of the process whose pages we are unmapping
 * OUTPUTS: -1 if the PID is invalid and 0 on success
 */
int32_t unmap_process(int32_t pid) {
	pcb_t *pcb = get_pcb_from_pid(pid);
	if (pcb == NULL)
		return -1;

	// Iterate through all the page mappings for this PID
	large_page_mapping_list_item *cur;
	for (cur = pcb->page_mappings_head; cur != NULL; cur = cur->next) {
		// Simply unmap the region
		unmap_region((void*)(cur->data.virt_index * LARGE_PAGE_SIZE), 1);
	}

	return 0;
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

	// Unmap video memory if it was mapped in
	if (pcb->vid_mem != NULL)
		unmap_video_mem_user(pcb->vid_mem);

	// Close all files and delete the files table
	int i;
	for (i = 0; i < pcb->files.length; i++) {
		if (pcb->files.data[i].fd_table->close != NULL)
			pcb->files.data[i].fd_table->close(i);
	}
	DYN_ARR_DELETE(pcb->files);

	// Unmap the pages for this process
	unmap_process(pcb->pid);

	// Free the page mapping linked list
	FREE_LIST(large_page_mapping_list_item, pcb->page_mappings_head);

	// Mark the current PID as unused and attempt to remove items from the end of the pcbs array
	pcb->pid = -1;
	for (i = pcbs.length - 1; i >= 0; i--) {
		if (pcbs.data[i].pid == -1)
			DYN_ARR_POP(pcb_t, pcbs);
	}

	// If the parent PID is -1, that means that this is the parent shell and we should
	//  simply spawn a new shell
	if (pcb->parent_pid == -1)
		process_execute("shell", 0);

	// Otherwise, we have a normal process that has a parent
	// Map the parent process in
	map_process(pcb->parent_pid);

	// Compute the address of the top of the parent's kernel stack
	uint32_t esp = KERNEL_START_ADDR + LARGE_PAGE_SIZE - KERNEL_STACK_SIZE * (1 + pcb->parent_pid);

	// Set TSS.ESP0 to point to where the top of the stack will be, containing only the PID
	tss.esp0 = esp - sizeof(int32_t);

	// Push esp down to where it was when execute() was called
	// The stack will contain, in order:
	//  - the PID
	//  - since it is a privilege switching interrupt: userspace DS segment selector, userspace esp
	//  - EFLAGS, userspace CS segment selector, userspace return address
	//  - 3 saved 32-bit registers
	//  - 3 32-bit parameters from registers edx, ecx, ebx
	//  - a 32-bit return address
	//  - possibly some other stuff that we don't care about
	// In total, one PID and twelve 32-bit values (that we care about)
	// So we can decrement esp by the corresponding amount
	esp = esp - sizeof(int32_t) - 12 * 4;
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
 * INPUTS: command: a shell command string; should only contain printable characters, and
 *                  should not contain any whitespace characters other than spaces
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

	// Check if the program has arguments, by checking for a space after the executable filename
	char arg_buf[TERMINAL_SIZE];
	int has_arguments = 0;
	if (command[i] == ' ') {
		// Skip any more spaces 
		for (; i < TERMINAL_SIZE && command[i] == ' '; i++);

		// The index in command where the arguments begin
		int start_of_arg = i;

		// Copy the arguments over from the command to arg_buf
		for (; i < TERMINAL_SIZE && command[i] != '\0'; i++) {
			arg_buf[i - start_of_arg] = command[i];
		}

		// Null terminate the string (this cannot buffer overflow since arg_buf is always big enough)
		arg_buf[i - start_of_arg] = '\0';

		// Keep track of whether or not there were arguments 
		has_arguments = i > start_of_arg;
	}

	// Get the PID for this process
	int32_t cur_pid = get_open_pid();
	if (cur_pid < 0) {
		sti();
		return -1;
	}

	// Get the PID of the current parent process (if it exists) before 
	//  we remap the pages to point to the new process
	pcb_t *parent_pcb = get_pcb();
	int32_t parent_pid = has_parent ? parent_pcb->pid : -1;

	// Get a physical 4MB page for the executable
	int page_index = get_open_page();	

	// Get the memory address where the executable will be placed and the page that contains it
	void *program_page = (void*)(LARGE_PAGE_SIZE * page_index);
	void *virt_prog_page = (void*)EXECUTABLE_VIRT_PAGE_START;
	void *virt_prog_location = (void*)EXECUTABLE_VIRT_PAGE_START + EXECUTABLE_PAGE_OFFSET;

	// Forcibly page in the memory region where the executable will be located so that we can write the executable
	// First, unmap the parent process' memory
	unmap_process(parent_pid);
	// Then, map in the single 4MB page for this process
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

	// Decrement esp0 to store the PID at the base of the stack, and store the PID there
	tss.esp0 -= sizeof(int32_t);
	*(int32_t*)tss.esp0 = cur_pid;

	// Get the PCB from the array that has a free spot at cur_pid
	pcb_t *pcb = &pcbs.data[cur_pid];

	// Initialize the fields of the PCB
	pcb->pid = cur_pid;
	pcb->parent_pid = parent_pid;
	pcb->vid_mem = NULL;

	// Create file_t objects for stdin and stdout
	file_t stdin_file, stdout_file;
	stdin_file.in_use = 1;
	stdin_file.fd_table = &stdin_table;
	stdout_file.in_use = 1;
	stdout_file.fd_table = &stdout_table;

	// Then, initialize the files dynamic array and add the two elements, checking all allocations on the way
	DYN_ARR_INIT(file_t, pcb->files);
	if (pcb->files.data == NULL)
		goto process_execute_fail;
	// Adds the two elements, relying on short circuit evaluation to break if pushing fails
	if (DYN_ARR_PUSH(file_t, pcb->files, stdin_file) < 0 || DYN_ARR_PUSH(file_t, pcb->files, stdout_file) < 0) {
		DYN_ARR_DELETE(pcb->files);
		goto process_execute_fail;
	}

	// Set the single page allocated to this process as the head of the pages linked list
	pcb->page_mappings_head = kmalloc(sizeof(large_page_mapping_list_item));
	// Check for NULL and free all previously allocated memory if so
	if (pcb->page_mappings_head == NULL) {
		DYN_ARR_DELETE(pcb->files);
		goto process_execute_fail;
	}
	// Otherwise, continue setting the index of the page
	pcb->page_mappings_head->data.virt_index = EXECUTABLE_VIRT_PAGE_START / LARGE_PAGE_SIZE;
	pcb->page_mappings_head->data.phys_index = page_index;
	pcb->page_mappings_head->next = NULL;
	
	// Copy the arguments into the PCB
	if (has_arguments) {
		for (i = 0; i < TERMINAL_SIZE && arg_buf[i] != '\0'; i++)
			pcb->args[i] = arg_buf[i];
		// Null terminate the string
		pcb->args[i] = '\0';
	} else {
		// Clear the arguments buffer if there are no arguments
		for (i = 0; i < TERMINAL_SIZE; i++)
			pcb->args[i] = '\0';
	}

	// We are switching to userspace, so note this down for bookkeeping
	in_userspace = 1;

	// Push parameters onto stack for IRET into userspace
	// First, set ds, es, fs, gs to the correct values
	// Then, push the DS as well as the ESP
	// Then, push the value of the flags register and enable IF
	// Lastly, push CS and the address to jump to in userspace
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

	// An idiomatic way to use gotos in C is error handling
process_execute_fail:
	// Unmap the memory mapped in for this process
	unmap_region(virt_prog_page, 1);

	// Mark the page set aside for this process as unused
	free_page(page_index);

	// Map in the memory for the parent process
	// Note that this doesn't make sense for the first process, which has no parent,
	//  so we better not arrive here when we're launching the first process
	map_process(parent_pid);

	// Set the PID as unused
	pcbs.data[cur_pid].pid = -1;

	// Restore interrupts and return -1
	sti();
	return -1;
}
