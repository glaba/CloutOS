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

// Pointers to the video memory back buffers for each of the TTYs, which programs will draw to
//  when their TTY is not active
static void *vid_mem_buffers[NUM_TEXT_TTYS];
// Indicates whether or not the shell program has been started for the TTY of each index
static int tty_inited[NUM_TEXT_TTYS];

// The currently active TTY
uint8_t active_tty = 1;

/*
 * Initializes any supporting data structures for managing user level processes
 *
 * OUTPUTS: -1 on critical failure that should stop the OS (very extremely unlikely), 0 otherwise
 */
int init_processes() {
	// Initialize the PCB array
	DYN_ARR_INIT(pcb_t, pcbs);
	if (pcbs.data == NULL)
		return -1;

	// Create video memory buffers for the 3 text TTYs
	// We will place all of them within one single 4MB page
	int32_t vid_mem_buffer_page = get_open_page();
	if (vid_mem_buffer_page == -1)
		return -1;

	// Map in the page
	identity_map_containing_region((void*)(vid_mem_buffer_page * LARGE_PAGE_SIZE), LARGE_PAGE_SIZE,
		PAGE_GLOBAL | PAGE_READ_WRITE);

	// The buffers will simply be sequential within the 4MB page
	int i;
	for (i = 0; i < NUM_TEXT_TTYS; i++)
		vid_mem_buffers[i] = (void*)(vid_mem_buffer_page * LARGE_PAGE_SIZE + VIDEO_SIZE * i);

	// Clear the TTYs that are not currently active
	for (i = 0; i < NUM_TEXT_TTYS; i++) {
		if (i + 1 != active_tty)
			clear_tty(i + 1);
	}

	// List TTY2 and TTY3 as uninitialized
	tty_inited[0] = 1;
	for (i = 1; i < NUM_TEXT_TTYS; i++) {
		tty_inited[i] = 0;
	}

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
 * Gets the pointer to the start of video memory for the given TTY
 *
 * INPUTS: TTY: the TTY to get the video memory for
 * OUTPUTS: a pointer to either video memory itself, or a buffer of the same size that the inactive
 *          provided TTY is writing to
 */
void *get_vid_mem(uint8_t tty) {
	// Check that TTY is valid
	if (tty < 1 || tty > NUM_TEXT_TTYS)
		return NULL;

	if (active_tty == tty)
		return (void*)VIDEO;
	else
		return vid_mem_buffers[tty - 1];
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
	pcb_t *pcb = get_pcb_from_pid(pid);

	// Go through the list of allocated pages
	int i;
	for (i = 0; i < pcb->large_page_mappings.length; i++) {
		uint32_t start_addr = pcb->large_page_mappings.data[i].virt_index * LARGE_PAGE_SIZE;

		// Check if it is within the virtual address given by this page
		if (((uint32_t)ptr < start_addr) || 
		    ((uint32_t)ptr + size >= start_addr + LARGE_PAGE_SIZE))
			return -1;
	}

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
	// Get the size of the string (including the \0, so we start counting at 1)
	int size;
	for (size = 1; *(uint8_t*)ptr != '\0'; ptr++, size++);

	return is_userspace_region_valid(ptr, size, pid);
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

	// Iterate through all the large page mappings for this PID
	int i;
	for (i = 0; i < pcb->large_page_mappings.length; i++) {
		// Unmap the region first, since map_region checks if the region is already paged in
		unmap_region((void*)(pcb->large_page_mappings.data[i].virt_index * LARGE_PAGE_SIZE), 1);
		// Map in the single 4MB region
		map_region((void*)(pcb->large_page_mappings.data[i].phys_index * LARGE_PAGE_SIZE), 
			(void*)(pcb->large_page_mappings.data[i].virt_index * LARGE_PAGE_SIZE), 
			1, PAGE_READ_WRITE | PAGE_USER_LEVEL);
	}

	// Map in video memory if it is supposed to be mapped in
	if (pcb->vid_mem != NULL) {
		// Check whether it should write to video memory or a buffer, and get
		//  the physical address corresponding to the correct option
		void *phys_addr = get_vid_mem(pcb->tty);

		map_video_mem_user(phys_addr);
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

	// Iterate through all the large page mappings for this PID
	int i;
	for (i = 0; i < pcb->large_page_mappings.length; i++) {
		// Unmap the region first, since map_region checks if the region is already paged in
		unmap_region((void*)(pcb->large_page_mappings.data[i].virt_index * LARGE_PAGE_SIZE), 1);
	}

	// Unmap video memory if it was mapped in
	if (pcb->vid_mem != NULL) {
		unmap_video_mem_user();
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
	// Stop any scheduling interrupts while this is happening
	cli();

	// Get the PCB corresponding to this kernel stack
	pcb_t *pcb = get_pcb();

	// Store the top of the kernel stack and the current TTY
	void *kernel_stack_top = pcb->kernel_stack_base - KERNEL_STACK_SIZE;
	uint8_t tty = pcb->tty;

	// Unmap video memory if it was mapped in
	if (pcb->vid_mem != NULL)
		unmap_video_mem_user();

	// Close all files and delete the files table
	int i;
	for (i = 0; i < pcb->files.length; i++) {
		if (pcb->files.data[i].fd_table->close != NULL)
			pcb->files.data[i].fd_table->close(i);
	}
	DYN_ARR_DELETE(pcb->files);

	// Unmap the pages for this process
	unmap_process(pcb->pid);

	// Free all the pages for this process
	for (i = 0; i < pcb->large_page_mappings.length; i++)
		free_page(pcb->large_page_mappings.data[i].phys_index);

	// Free the page mapping dynamic array
	DYN_ARR_DELETE(pcb->large_page_mappings);

	// Store the parent PID from the PCB before deleting it
	int32_t parent_pid = pcb->parent_pid;

	// Mark the current PID as unused and attempt to remove items from the end of the pcbs array
	pcb->pid = -1;
	for (i = pcbs.length - 1; i >= 0; i--) {
		if (pcbs.data[i].pid == -1) {
			PROC_DEBUG("Removing stale PCB that corresponded to PID %d\n", i);
			DYN_ARR_POP(pcb_t, pcbs);
		} else {
			break;
		}
	}

	// If the parent PID is -1, that means that this is the parent shell and we should
	//  simply spawn a new shell
	if (parent_pid == -1) {
		// Free the kernel stack, knowing that interrupts are blocked to prevent the stack from being allocated
		//  while we use it for some more time
		kfree(kernel_stack_top);
		// Spawn a new shell in the same TTY
		process_execute("shell", 0, tty, 0);
	}

	// Otherwise, we have a normal process that has a parent
	// Map the parent process in
	map_process(parent_pid);

	// Set the parent process as RUNNING instead of SLEEPING
	pcbs.data[parent_pid].state = PROCESS_RUNNING;

	// Get the address of the top of the parent process' kernel stack
	uint32_t esp = (uint32_t)pcbs.data[parent_pid].kernel_stack_base;

	// Set TSS.ESP0 to point to where the top of the stack will be, containing only the PID
	tss.esp0 = esp - sizeof(int32_t);

	// Set TSS.SS0 as well 
	tss.ss0 = KERNEL_DS;

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

	// Lastly, we need to free the current kernel stack
	// We know that interrupts are blocked for the time being to prevent the freed stack from being allocated
	//  during an interrupt and used (potentially messing up the execution of this function)
	kfree(kernel_stack_top);

	// Set eax to the desired return value, set esp to the the esp of the parent process
	//  and return back to the assembly linkage
	// Re-enable interrupts now that we have switched stacks entirely
	asm volatile ("      \n\
		movl %k0, %%eax  \n\
		movl %k1, %%esp  \n\
		sti              \n\
		ret"
		:
		: "r"(((uint32_t)status)), "r"((esp))
		: "eax", "esp" // An entirely insufficient and pointless list
	);

	// Placeholder to get gcc to shut up
	return 0;
}

/*
 * Starts the process associated with the given shell command
 * INPUTS: command: a shell command string; should only contain printable characters, and
 *                  should not contain any whitespace characters other than spaces
 *         has_parent: 1 if the newly created process has a parent, and 0 if not
 *         tty: if has_parent is 0, the TTY that this process should be in
 *         save_context: whether or not to save the context in the current kernel stack's PCB
 */
int32_t process_execute(const char *command, uint8_t has_parent, uint8_t tty, uint8_t save_context) {
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

	// Get the PID of the current parent process (if it exists) as well as the parent TTY before 
	//  we remap the pages to point to the new process
	pcb_t *parent_pcb = get_pcb();
	int32_t parent_pid = has_parent ? parent_pcb->pid : -1;
	uint8_t parent_tty = has_parent ? parent_pcb->tty : tty;
	
	// If the parent process exists, mark it as SLEEPING, since it will be blocking 
	//  on process_execute for as long as the new child process runs
	if (has_parent)
		parent_pcb->state = PROCESS_SLEEPING;

	// Get a physical 4MB page for the executable
	int page_index = get_open_page();	

	// Get the memory address where the executable will be placed and the page that contains it
	void *program_page = (void*)(LARGE_PAGE_SIZE * page_index);
	void *virt_prog_page = (void*)EXECUTABLE_VIRT_PAGE_START;
	void *virt_prog_location = (void*)EXECUTABLE_VIRT_PAGE_START + EXECUTABLE_PAGE_OFFSET;

	// Forcibly page in the memory region where the executable will be located so that we can write the executable
	// First, unmap the parent process' memory if has_parent is true OR if save_context is true
	if (has_parent || save_context)
		unmap_process(parent_pcb->pid);
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
	// ESP0 should point to the 8KB kernel stack for this process
	//  which we will allocate now (must be 8KB aligned as well)
	void *kernel_stack_base = kmalloc_aligned(KERNEL_STACK_SIZE, KERNEL_STACK_SIZE) + KERNEL_STACK_SIZE;
	tss.esp0 = (uint32_t)kernel_stack_base;
	// Check for null as with any dynamic allocation
	if (tss.esp0 == KERNEL_STACK_SIZE) {
		goto process_execute_fail;
	}

	// Decrement esp0 to store the PID at the base of the stack, and store the PID there
	tss.esp0 -= sizeof(int32_t);
	*(int32_t*)tss.esp0 = cur_pid;

	// Get the PCB from the array that has a free spot at cur_pid
	pcb_t *pcb = &pcbs.data[cur_pid];

	// Initialize the fields of the PCB
	pcb->pid = cur_pid;
	pcb->tty = parent_tty;
	pcb->state = PROCESS_RUNNING;
	pcb->parent_pid = parent_pid;
	pcb->vid_mem = NULL;
	pcb->kernel_stack_base = kernel_stack_base;

	// Create file_t objects for stdin and stdout
	file_t stdin_file, stdout_file;
	stdin_file.in_use = 1;
	stdin_file.fd_table = &stdin_table;
	stdout_file.in_use = 1;
	stdout_file.fd_table = &stdout_table;

	// Then, initialize the files dynamic array and add the two elements, checking all allocations on the way
	DYN_ARR_INIT(file_t, pcb->files);
	if (pcb->files.data == NULL) {
		kfree(kernel_stack_base - KERNEL_STACK_SIZE);
		goto process_execute_fail;
	}
	// Adds the two elements, relying on short circuit evaluation to break if pushing fails
	if (DYN_ARR_PUSH(file_t, pcb->files, stdin_file) < 0 || DYN_ARR_PUSH(file_t, pcb->files, stdout_file) < 0) {
		kfree(kernel_stack_base - KERNEL_STACK_SIZE);
		DYN_ARR_DELETE(pcb->files);
		goto process_execute_fail;
	}

	// Initialize the large_page_mappings array
	DYN_ARR_INIT(page_mapping, pcb->large_page_mappings);
	// Check for NULL and free all previously allocated memory if so
	if (pcb->large_page_mappings.data == NULL) {
		kfree(kernel_stack_base - KERNEL_STACK_SIZE);
		DYN_ARR_DELETE(pcb->files);
		goto process_execute_fail;
	}
	// Add one entry for the single page allocated to this process
	page_mapping page;
	page.virt_index = EXECUTABLE_VIRT_PAGE_START / LARGE_PAGE_SIZE;
	page.phys_index = page_index;
	// This call cannot fail because dynamic arrays are initialized with a non-zero capacity
	DYN_ARR_PUSH(page_mapping, pcb->large_page_mappings, page);
	
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

	// Print the PID of this process
	PROC_DEBUG("Starting process with PID %d\n", cur_pid);

	// We are switching to userspace, so note this down for bookkeeping
	in_userspace = 1;

	// If we want to save the context, store a pointer to the label process_execute_label in the EIP 
	//  field, which is where we will return to
	if (save_context)
		parent_pcb->context.eip = (uint32_t)(&&process_execute_return);

	// Save the context if desired
	//  which involves putting ESP and EBP into the context struct at offsets 0 and 4 respectively
	// Push parameters onto stack for IRET into userspace
	// First, set ds, es, fs, gs to the correct values
	// Then, push the DS as well as the ESP
	// Then, push the value of the flags register and enable IF
	// Then, push CS and the address to jump to in userspace
	// Lastly, re-enable interrupts (an interrupt here won't break things)
	//  and jump into userspace with iret
	asm volatile ("          \n\
		cmpl $0, %k5         \n\
		je dont_save_context \n\
		pushal               \n\
		pushfl               \n\
		movl %%esp, 0(%k4)   \n\
		movl %%ebp, 4(%k4)   \n\
	dont_save_context:       \n\
		movl %k0, %%eax      \n\
		movw %%ax, %%ds      \n\
		movw %%ax, %%es      \n\
		movw %%ax, %%fs      \n\
		movw %%ax, %%gs      \n\
		pushl %k0            \n\
		pushl %k1            \n\
		pushfl               \n\
		orl $0x200, (%%esp)  \n\
		pushl %k2            \n\
		pushl %k3"
		:
		: "r"((USER_DS)), "r"((program_esp)), "r"((USER_CS)), "r"((entrypoint)), 
		  "r"((&parent_pcb->context)), "r"((save_context))
	);

	asm volatile ("iret");

	// The code will jump here after a child process halts or after execution returns to the program that was
	//  running when a TTY switch occurred
	// The latter occurs in this scenario: shells in TTY2 and TTY3 have not yet been started, and the user
	//  presses ALT+2. The kernel calls process_execute to create a shell in TTY2 within the kernel stack
	//  of whatever process was running at the time. When the scheduler returns back to that process, it will
	//  return back to this label, which will return back to the keyboard handler, which will then return back
	//  to the user program
process_execute_return:
	// Reload the general purpose registers and flags
	asm volatile ("popfl; popal");

	return 0;

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

	// Mark the parent process as running again
	parent_pcb->state = PROCESS_RUNNING;

	// Set the PID as unused
	pcbs.data[cur_pid].pid = -1;

	// Restore interrupts and return -1
	sti();
	return -1;
}

/*
 * Maps video memory for the current userspace program to either video memory or a buffer depending
 *  on whether or not the current program is in the active TTY
 * Must be called from the kernel stack of a userspace program
 *
 * OUTPUTS: screen_start: a pointer to pointer that contains the virtual address of video memory
 *                        after it is paged in
 * SIDE EFFECTS: may modify the page directory
 */
int32_t process_vidmap(uint8_t **screen_start) {
	pcb_t *pcb = get_pcb();

	// Check if video is already mapped in
	if (pcb->vid_mem != NULL)
		return -1;

	// Check that the provided pointer is valid
	if (is_userspace_region_valid((void*)screen_start, 1, pcb->pid) == -1)
		return -1;

	// Check whether or not the process should be writing directly to video memory
	//  or if it should be writing to a buffer (based on whether or not it is in the active TTY)
	void *phys_addr = get_vid_mem(pcb->tty);

	int32_t retval = map_video_mem_user(phys_addr);

	if (retval == -1)
		return -1;

	// Copy the value into the PCB and the return value
	*screen_start = (uint8_t*)VIDEO_USER_VIRT_ADDR;
	get_pcb()->vid_mem = (void*)VIDEO_USER_VIRT_ADDR;

	// Return success
	return 0;
}

/*
 * Switches from the current TTY to the provided TTY
 * Must be called from the kernel stack of a userspace program
 *
 * INPUTS: tty: the TTY that we want to switch to
 * OUTPUTS: -1 if the tty was invalid / we couldn't switch for some reason, and 0 on success
 */
int32_t tty_switch(uint8_t tty) {
	if (tty == 0 || tty > NUM_TEXT_TTYS)
		return -1;

	// We don't want a scheduling / keyboard interrupt to occur while we're copying buffers over
	//  or modifying active_tty
	cli();

	uint8_t *vid_mem = (uint8_t*)VIDEO;
	uint8_t *active_tty_buffer = (uint8_t*)vid_mem_buffers[active_tty - 1];
	uint8_t *new_tty_buffer = (uint8_t*)vid_mem_buffers[tty - 1];

	// Copy video memory into the buffer for active_tty (the old TTY)
	//  and copy the buffer for tty into video memory
	int i;
	for (i = 0; i < VIDEO_SIZE; i++) {
		active_tty_buffer[i] = vid_mem[i];
		vid_mem[i] = new_tty_buffer[i];
	}

	// Remap the video memory for the currently running process if it is in the old TTY
	//  or if it's in the new TTY
	pcb_t *pcb = get_pcb();
	if (pcb->vid_mem != NULL) {
		unmap_video_mem_user();
		
		// If its TTY is not the new TTY, have it write to the buffer for that TTY
		if (pcb->tty != tty)
			map_video_mem_user(vid_mem_buffers[pcb->tty - 1]);
		// Otherwise, we are switching to this process' TTY, and it should map directly to video memory
		else
			map_video_mem_user(vid_mem);
	}

	// Update the active TTY
	active_tty = tty;

	// Restore interrupts now that we're done copying the buffers over
	sti();

	// Update the cursor position
	update_cursor();

	// If there is no shell running in this TTY, start one, saving the context so that we can correctly
	//  return back to here
	if (!tty_inited[tty - 1]) {
		tty_inited[tty - 1] = 1;
		process_execute("shell", 0, tty, 1);
	}

	return 0;
}

/*
 * Switches from the currently running userspace program to the program with the given PID
 * Must be called from the kernel stack of a userspace program
 * 
 * INPUTS: pid: the PID of the program to switch to
 * OUTPUTS: -1 if the context switch could not be completed and 0 if it was
 */
int32_t context_switch(int32_t pid) {
	// Check that the PID is valid
	if (pid < 0 || pid >= pcbs.length)
		return -1;

	// Check that the PID represents an existing process
	if (pcbs.data[pid].pid == -1)
		return -1;

	// Get the PCB for the current process and the process we are switching to
	pcb_t *old_pcb = get_pcb();
	pcb_t *new_pcb = get_pcb_from_pid(pid);

	// Unmap the memory for the previous process and map in the memory for the new one
	// This should handle mapping in video memory as well
	unmap_process(old_pcb->pid);
	map_process(new_pcb->pid);

	// Set the TSS ESP0 and SS0 entries for the new process
	tss.esp0 = (uint32_t)new_pcb->kernel_stack_base - sizeof(uint32_t);
	tss.ss0 = KERNEL_DS;

	// Get the address that we will return to when we come back to this process
	//  && is a GCC-specific operator that gets the address of a label
	old_pcb->context.eip = (uint32_t)(&&context_switch_return);

	// Push all general purpose registers and flags
	// Copy the ESP and EBP to return into this process' PCB
	//  and restore the ESP, EBP for the next process
	// Finally, push EIP for the next process onto the stack, and return to it
	// The offsets into the struct for esp, ebp, and eip are 0, 4, 8 respectively
	asm volatile ("         \n\
		pushal              \n\
		pushfl              \n\
		movl %%esp, 0(%k0)  \n\
		movl %%ebp, 4(%k0)  \n\
		movl 0(%k1), %%esp  \n\
		movl 4(%k1), %%ebp  \n\
		pushl 8(%k1)        \n\
		ret"
		:
		: "r"((&old_pcb->context)), "r"((&new_pcb->context))
	);

context_switch_return:
	// Reload the general purpose registers and flags
	asm volatile ("popfl; popal");

	return 0;
}

/*
 * Handler called by timer that switches to the next process in a round robin fashion
 */
void scheduler_interrupt_handler() {
	// We don't want the scheduler to be interrupted by anything, it should be fast
	cli();

	// If there are no running processes, exit
	if (pcbs.length == 0)
		return;

	// Get the current PCB and the current PID, we will just go to the next PID
	pcb_t *pcb = get_pcb();
	int pid = pcb->pid;

	// Iterate through the pcbs array until we find an existing process that is in 
	//  the state PROCESS_RUNNING, which means we can switch to it
	int i, next_pid;
	i = (pid + 1) % pcbs.length;
	next_pid = -1;
	for (i = (pid + 1) % pcbs.length; i != pid; i = (i + 1) % pcbs.length) {
		if (pcbs.data[i].pid >= 0 && pcbs.data[i].state == PROCESS_RUNNING) {
			// Set this as the process we will switch to
			next_pid = i;
			break;
		}
	}

	// If we found no process to switch to, just return and keep going with this process
	if (next_pid == -1)
		return;

	// Otherwise, context switch to that process
	context_switch(next_pid);
}
