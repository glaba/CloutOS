#include "processes.h"
#include "lib.h"
#include "file_system.h"
#include "paging.h"
#include "x86_desc.h"
#include "kheap.h"
#include "i8259.h"
#include "interrupt_service_routines.h"
#include "graphics/graphics.h"
#include "graphics/VMwareSVGA.h"
#include "window_manager/window_manager.h"

// A dynamic array indicating which PIDs are currently in use by running programs
// Each index corresponds to a PID and contains a pointer to that process' PCB
pcb_dyn_arr pcbs;

// A spinlock that prevents the pcbs dynamic array from being modified
struct spinlock_t pcb_spin_lock = SPIN_LOCK_UNLOCKED;

static struct fops_t stdin_table  = {.open = NULL, .close = NULL,
	                                 .read = (int32_t (*)(int32_t, void*, int32_t))&terminal_read,
	                                 .write = NULL};
static struct fops_t stdout_table = {.open = NULL, .close = NULL,
	                                 .read = NULL,
	                                 .write = (int32_t (*)(int32_t, const void*, int32_t))&terminal_write};

// Pointers to the video memory back buffers for each of the TTYs, which programs will draw to
//  when their TTY is not active
void *vid_mem_buffers[NUM_TTYS];

// A spinlock that prevents the TTY from changing while it is owned
struct spinlock_t tty_spin_lock = SPIN_LOCK_UNLOCKED;

// Indicates whether or not the shell program has been started for the TTY of each index
static int shell_started[NUM_TEXT_TTYS];

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
	// We will place each of them within its own page (12 MB total)
	int i;
	for (i = 0; i < NUM_TTYS; i++) {
		int32_t vid_mem_buffer_page = get_open_page();
		if (vid_mem_buffer_page == -1)
			return -1;
		
		// Map in the page
		identity_map_containing_region((void*)(vid_mem_buffer_page * LARGE_PAGE_SIZE), LARGE_PAGE_SIZE,
			PAGE_GLOBAL | PAGE_READ_WRITE);
		
		// Store a pointer to the buffer
		vid_mem_buffers[i] = (void*)(vid_mem_buffer_page * LARGE_PAGE_SIZE);
	}

	// Clear the TTYs that are not currently active
	for (i = 0; i < NUM_TEXT_TTYS; i++) {
		if (i + 1 != active_tty)
			clear_tty(i + 1);
	}

	// List TTY2 and TTY3 as uninitialized
	shell_started[0] = 1;
	for (i = 1; i < NUM_TEXT_TTYS; i++) {
		shell_started[i] = 0;
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
 * Returns an unused PID, and marks it used
 * If none can be found, it returns -1
 *
 * SIDE EFFECTS: may resize pcbs if there are no unused PIDs
 */
int32_t get_open_pid() {
	spin_lock_irqsave(pcb_spin_lock);

	int i;
	for (i = 0; i < pcbs.length; i++) {
		if (pcbs.data[i].pid < 0) {
			pcbs.data[i].pid = i;
			spin_unlock_irqsave(pcb_spin_lock);
			return i;
		}
	}

	// If we got this far, that means no PID was found
	// Add a new empty entry to the dynamic array
	pcb_t new_pcb;
	new_pcb.pid = pcbs.length;
	int pid = DYN_ARR_PUSH(pcb_t, pcbs, new_pcb);

	spin_unlock_irqsave(pcb_spin_lock);

	// Return the pid
	// If pushing to pcbs failed, it will be -1 which already means failure, so we need no more checks
	return pid;
}

/*
 * Gets the PCB corresponding to the given PID
 * pcb_spin_lock should be locked before calling this function
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
 * Gets the current PID from tss.esp0
 */
int32_t get_pid() {
	// Initialize a pointer to the PID which is stored at the base of the kernel stack
	void* addr_of_pid;
	// Mask out lower 8 kB to get to prev 8kB
	addr_of_pid = (void*)(tss.esp0 & KERNEL_STACK_BASE_BITMASK);
	// Add 8kB in order to round up to the base of the stack
	addr_of_pid += KERNEL_STACK_SIZE;
	// Decrement to get to the PID
	addr_of_pid -= sizeof(int32_t);
	// Return the PID at the address we discovered
	return *(int32_t*)addr_of_pid;
}

/*
 * Get the current pcb from tss.esp0
 * pcb_spin_lock should be locked before calling this function
 *
 * OUTPUTS: the PCB corresponding to the current userspace program's PID
 */
pcb_t* get_pcb() {
	return get_pcb_from_pid(get_pid());
}

/*
 * Gets a pointer to the current userspace program's registers stored on the kernel stack
 *
 * OUTPUTS: a pointer to the context of the current userspace program, which if modified, will cause
 *          the userspace program to have the modified context when the kernel returns back to it
 */
process_context *get_user_context() {
	spin_lock_irqsave(pcb_spin_lock);

	process_context *context;

	// Get the base of the stack
	void *stack_base = get_pcb()->kernel_stack_base;

	// Subtract sizeof(int32_t) to get the base of the stack excluding the PID
	stack_base -= sizeof(int32_t);
	// Subtract sizeof(process_context) to get the base of the process_context
	stack_base -= sizeof(process_context);

	// Cast it to a process_context
	context = (process_context*)stack_base;

	spin_unlock_irqsave(pcb_spin_lock);
	return context;
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
	if (tty < 1 || tty > NUM_TTYS)
		return NULL;

	if (active_tty == tty) {
		if (vga_text_enabled)
			return (void*)VIDEO;
		else
			return (void*)svga.frame_buffer;
	} else
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
	spin_lock_irqsave(pcb_spin_lock);

	pcb_t *pcb = get_pcb_from_pid(pid);

	// Go through the list of allocated pages
	int i;
	for (i = 0; i < pcb->large_page_mappings.length; i++) {
		uint32_t start_addr = pcb->large_page_mappings.data[i].virt_index * LARGE_PAGE_SIZE;

		// Check if it is within the virtual address given by this page
		if (((uint32_t)ptr < start_addr) || 
		    ((uint32_t)ptr + size >= start_addr + LARGE_PAGE_SIZE)) {

			spin_unlock_irqsave(pcb_spin_lock);
			return -1;
		}
	}

	spin_unlock_irqsave(pcb_spin_lock);
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
	spin_lock_irqsave(pcb_spin_lock);

	pcb_t *pcb = get_pcb_from_pid(pid);
	if (pcb == NULL) {
		spin_unlock_irqsave(pcb_spin_lock);
		return -1;
	}

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
		// TODO
		// Check whether it should write to video memory or a buffer, and get
		//  the physical address corresponding to the correct option
		void *phys_addr = get_vid_mem(pcb->tty);

		map_video_mem_user(phys_addr); //TODO
	}

	spin_unlock_irqsave(pcb_spin_lock);
	return 0;
}

/*
 * Forcibly unmaps the pages allocated to the process with given PID
 *
 * INPUTS: pid: the PID of the process whose pages we are unmapping
 * OUTPUTS: -1 if the PID is invalid and 0 on success
 */
int32_t unmap_process(int32_t pid) {
	spin_lock_irqsave(pcb_spin_lock);

	pcb_t *pcb = get_pcb_from_pid(pid);
	if (pcb == NULL) {
		spin_unlock_irqsave(pcb_spin_lock);
		return -1;
	}

	// Iterate through all the large page mappings for this PID
	int i;
	for (i = 0; i < pcb->large_page_mappings.length; i++) {
		// Unmap the region first, since map_region checks if the region is already paged in
		unmap_region((void*)(pcb->large_page_mappings.data[i].virt_index * LARGE_PAGE_SIZE), 1);
	}

	// Unmap video memory if it was mapped in // TODO
	if (pcb->vid_mem != NULL) {
		unmap_video_mem_user();
	}

	spin_unlock_irqsave(pcb_spin_lock);
	return 0;
}

/*
 * Frees the resources consumed by the process of given PID and removes it from the PCBs array
 * WARNING: in general, the process cannot be the one whose kernel stack we are currently running on
 * There are very few scenarios where it can be, and it should be used in that case very cautiously
 *
 * INPUTS: pid: the PID of the process to free
 * OUTPUTS: -1 if the PID is invalid or the one whose kernel stack we're on, and 0 otherwise
 */
int32_t free_pid(int32_t pid) {
	spin_lock_irqsave(pcb_spin_lock);

	// Set TSS.esp0 to that of the process we are trying to free so that get_pid() and 
	//  get_pcb() return the right thing
	uint32_t saved_esp0 = tss.esp0;
	tss.esp0 = (uint32_t)(get_pcb_from_pid(pid)->kernel_stack_base - sizeof(uint32_t));

	pcb_t *pcb = get_pcb_from_pid(pid);

	// Store the top of the kernel stack and the current TTY
	void *kernel_stack_top = pcb->kernel_stack_base - KERNEL_STACK_SIZE;

	// Close all files and delete the files table
	int i;
	for (i = 0; i < pcb->files.length; i++) {
		if (pcb->files.data[i].in_use && pcb->files.data[i].fd_table->close != NULL)
			pcb->files.data[i].fd_table->close(i);
	}
	DYN_ARR_DELETE(pcb->files);

	// Free all the pages for this process
	for (i = 0; i < pcb->large_page_mappings.length; i++)
		free_page(pcb->large_page_mappings.data[i].phys_index);

	// Free the page mapping dynamic array
	DYN_ARR_DELETE(pcb->large_page_mappings);

	// Free the kernel stack for this process
	kfree(kernel_stack_top);

	// TODO maybe add stuff for window memory here

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

	tss.esp0 = saved_esp0;
	spin_unlock_irqsave(pcb_spin_lock);
	return 0;
}

/*
 * Halts the current process and returns the provided status code to the parent process
 * INPUTS: status: the status with which the program existed (256 for exception, [0-256) otherwise)
 * SIDE EFFECTS: it jumps to the kernel stack for the parent process and returns
*               the correct status value from execute
 */
int32_t process_halt(uint16_t status) {
	spin_lock_irqsave(pcb_spin_lock);

	// Get the PCB corresponding to this kernel stack
	pcb_t *pcb = get_pcb();
	pcb_t *parent_pcb = get_pcb_from_pid(pcb->parent_pid);	

	// If the parent PID is -1, that means that this is the parent shell and we should
	//  simply spawn a new shell
	if (pcb->parent_pid == -1) {
		// Store the TTY
		uint8_t tty = pcb->tty;
		// Unmap the pages allocated to this process
		unmap_process(pcb->pid);
		// Free the PCB and all associated data
		// Notice that this also frees the kernel stack that we are currently on
		// However, interrupts are disabled due to the spin_lock_irqsave, and will remain disabled
		//  throughout the execution of process_execute until we jump into the new shell
		free_pid(pcb->pid);
		// Spawn a new shell in the same TTY
		process_execute("shell", 0, tty, 0);
	}

	// Set the parent process as RUNNING instead of SLEEPING
	parent_pcb->state = PROCESS_RUNNING;
	// Set the current process as STOPPING
	pcb->state = PROCESS_STOPPING;

	// Set the blocking call data in the parent PCB to the status code
	parent_pcb->blocking_call.data = status;

	// Unlock the pcb spinlock now that we are done using it
	spin_unlock_irqsave(pcb_spin_lock);

	// Spin and wait for scheduler
	sti();
	while (1);

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
	if (cur_pid < 0)
		return -1;

	spin_lock_irqsave(pcb_spin_lock);

	// Get the PID of the current parent process (if it exists) as well as the parent TTY before 
	//  we remap the pages to point to the new process
	pcb_t *parent_pcb = get_pcb();
	int32_t parent_pid = has_parent ? parent_pcb->pid : -1;
	uint8_t parent_tty = has_parent ? parent_pcb->tty : tty;
	
	// If the parent process exists, mark it as SLEEPING, and fill the blocking_call field
	//  since it will be blocking on process_execute for as long as the new child process runs
	if (has_parent) {
		parent_pcb->state = PROCESS_SLEEPING;
		parent_pcb->blocking_call.type = BLOCKING_CALL_PROCESS_EXEC;
	}

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
	pcb->vid_mem = NULL; //TODO
	pcb->kernel_stack_base = kernel_stack_base;

	// Initialize the signal_handlers to NULL and signal_statuses to SIGNAL_OPEN
	for (i = 0; i < NUM_SIGNALS; i++) {
		pcb->signal_handlers[i] = NULL;
		pcb->signal_status[i] = SIGNAL_OPEN;
	}

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

	// We intentionally do not unlock pcb_spin_lock because it will get unlocked (read: sti will be called)
	//  when the jump into userspace occurs

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
	return get_pcb()->blocking_call.data;

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
	// We do not need a lock around this because no one would have made use of a PID
	//  marked as used anyway
	pcbs.data[cur_pid].pid = -1;

	spin_unlock_irqsave(pcb_spin_lock);

	// Restore interrupts and return -1
	return -1;
}

/*
 * Marks the provided process as asleep and spins until the current quantum is complete,
 *  in the case that the current quantum is the process being put to sleep
 *
 * INPUTS: pid: the PID of the process to put to sleep
 * OUTPUTS: 0 on success, which is always 
 */
int32_t process_sleep(int32_t pid) {
	spin_lock_irqsave(pcb_spin_lock);

	pcb_t *pcb = get_pcb_from_pid(pid);
	pcb->state = PROCESS_SLEEPING;

	spin_unlock_irqsave(pcb_spin_lock);

	// Spin while the process is in the sleep state 
	// The scheduler will take us out of this loop
	int sleeping = 1;
	while (sleeping) {
		spin_lock_irqsave(pcb_spin_lock);
		sleeping = (get_pcb_from_pid(pid)->state == PROCESS_SLEEPING);
		spin_unlock_irqsave(pcb_spin_lock);
	}

	// Return when the scheduler returns back to this process and it is awake
	return 0;
}

/*
 * Wakes up the process of provided PID
 * 
 * INPUTS: pid: the PID of the process to put to sleep
 * OUTPUTS: 0 on success, which is always
 */
int32_t process_wake(int32_t pid) {
	spin_lock_irqsave(pcb_spin_lock);

	pcb_t *pcb = get_pcb_from_pid(pid);
	pcb->state = PROCESS_RUNNING;

	spin_unlock_irqsave(pcb_spin_lock);

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
	if (tty == 0 || tty > NUM_TEXT_TTYS + 1)
		return -1;

	spin_lock_irqsave(tty_spin_lock);
	if (tty == 4) {
		GUI_enabled = 1;
		// init_desktop();
		int32_t old_tty = active_tty;
		uint32_t *vid_mem = (uint32_t*)svga.frame_buffer;
		uint32_t *old_tty_buffer = (uint32_t*)vid_mem_buffers[old_tty - 1];
		uint32_t *new_tty_buffer = (uint32_t*)vid_mem_buffers[tty - 1];	
		memcpy(old_tty_buffer, vid_mem, svga.width * svga.height * 4);
		memcpy(vid_mem, new_tty_buffer, svga.width * svga.height * 4);	


		active_tty = tty;
		compositor();
		spin_unlock_irqsave(tty_spin_lock);
		sti();
		return 0;
	}
	GUI_enabled = 0;
	int32_t old_tty = active_tty;

	uint32_t *vid_mem = (uint32_t*)svga.frame_buffer;
	uint32_t *old_tty_buffer = (uint32_t*)vid_mem_buffers[old_tty - 1];
	uint32_t *new_tty_buffer = (uint32_t*)vid_mem_buffers[tty - 1];


	// BELOW METHOD WORKED FINE FOR TEXT MODE TTY, MEMCPY IS FASTER FOR GRAPHICS
	// Copy video memory into the buffer for active_tty (the old TTY)
	//  and copy the buffer for tty into video memory
	// int i;
	// for (i = 0; i < svga.width * svga.height * 4; i++) {
	// 	old_tty_buffer[i] = vid_mem[i];
	// 	vid_mem[i] = new_tty_buffer[i];
	// }
	memcpy(old_tty_buffer, vid_mem, svga.width * svga.height * 4);
	memcpy(vid_mem, new_tty_buffer, svga.width * svga.height * 4);

	spin_lock_irqsave(pcb_spin_lock);

	// Remap the video memory for the currently running process if it is in the old TTY
	//  or if it's in the new TTY
	pcb_t *pcb = get_pcb();
	if (pcb->vid_mem != NULL) {
		unmap_video_mem_user(); // TODO
		
		// If its TTY is not the new TTY, have it write to the buffer for that TTY
		if (pcb->tty != tty)
			map_video_mem_user(vid_mem_buffers[pcb->tty - 1]);
		// Otherwise, we are switching to this process' TTY, and it should map directly to video memory
		else
			map_video_mem_user(vid_mem);
	}

	spin_unlock_irqsave(pcb_spin_lock);

	// Update the active TTY
	active_tty = tty;

	// Unlock the spinlock now that we are done modifying active_tty and related data
	spin_unlock_irqsave(tty_spin_lock);

	// Update the cursor position
	update_cursor();

	// If there is no shell running in this TTY, start one, saving the context so that we can correctly
	//  return back to here
	if (tty <= NUM_TEXT_TTYS && !shell_started[tty - 1]) {
		shell_started[tty - 1] = 1;
		// Start the new shell
		process_execute("shell", 0, tty, 1);
	}

	sti();
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
	spin_lock_irqsave(pcb_spin_lock);

	// Check that the PID is valid
	if (pid < 0 || pid >= pcbs.length) {
		spin_unlock_irqsave(pcb_spin_lock);
		return -1;
	}

	// Check that the PID represents an existing process
	if (pcbs.data[pid].pid == -1 || pcbs.data[pid].state == PROCESS_STOPPING) {
		spin_unlock_irqsave(pcb_spin_lock);
		return -1;
	}

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

	// We don't unlock the spinlock because the inline assembly is still using the PCBs
	// Instead, we have a sti instruction in the assembly after using the PCBs
	// We cannot have a call to spin_unlock_irqsave because the stack has been messed up
	//  and the compiler generates bad code

	// Push all general purpose registers and flags
	// Copy the ESP and EBP to return into this process' PCB
	//  and restore the ESP, EBP for the next process
	// Push EIP for the next process onto the stack
	// The offsets into the struct for esp, ebp, and eip are 0, 4, 8 respectively
	asm volatile ("         \n\
		movl %%esp, 0(%k0)  \n\
		movl %%ebp, 4(%k0)  \n\
		movl 0(%k1), %%esp  \n\
		movl 4(%k1), %%ebp  \n\
		pushl 8(%k1)        \n\
		sti                 \n\
		ret"
		:
		: "r"((&old_pcb->context)), "r"((&new_pcb->context))
	);

context_switch_return:
	spin_lock_irqsave(pcb_spin_lock);
	
	if (timer_linkage_esp == tss.esp0)
		handle_signals();

	spin_unlock_irqsave(pcb_spin_lock);
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
	// Stop looping when we run into the current process
	int i, next_pid;
	next_pid = -1;
	for (i = (pid + 1) % pcbs.length; i != pid; i = (i + 1) % pcbs.length) {
		if (pcbs.data[i].pid >= 0 && pcbs.data[i].state == PROCESS_RUNNING) {
			// Set this as the next process
			next_pid = i;
			break;
		}

		// If we find a process that needs to be stopped, let's just clear it out
		if (pcbs.data[i].pid >= 0 && pcbs.data[i].pid != pid && pcbs.data[i].state == PROCESS_STOPPING) {
			free_pid(i);
		}
	}

	// If we found no process to switch to, just keep going with this process
	if (next_pid == -1) {
		// Handle signals only if the timer interrupt for scheduling is the only thing on the stack
		if (timer_linkage_esp == tss.esp0)
			handle_signals();
		sti();
		return;
	}

	// Otherwise, context switch to that process
	context_switch(next_pid);
}
