#include "system_calls.h"
#include "processes.h"
#include "keyboard.h"
#include "paging.h"

// Instead of return -1 or 0, used labels/macros
#define PASS 0
#define FAIL -1

/* Jump table for specific read/write/open/close functions */
static struct fops_t rtc_table = {.open = &rtc_open, .close = &rtc_close, .read = &rtc_read, .write = &rtc_write};
static struct fops_t file_table = {.open = &file_open, .close = &file_close, .read = &file_read, .write = &file_write};
static struct fops_t dir_table = {.open = &dir_open, .close = &dir_close, .read = &dir_read, .write = &dir_write};

/*
 * SYSTEM CALL that halts the currently running process with the specified status
 * INPUTS: status: a status code between 0 and 256 that indicates how the program exited
 * OUTPUTS: FAIL if halting the process failed, and PASS otherwise
 */
int32_t halt(uint32_t status) {
	return process_halt(status & 0xFF);
}

/*
 * SYSTEM CALL that executes the given shell command
 * INPUTS: command: a shell command
 * OUTPUTS: the status which with the program exits on completion
 */
int32_t execute(const char *command) {
	return process_execute(command, 1);
}

/*
 * SYSTEM CALL that then refers to the appropriate fd then invokes it
 * INPUTS: int32_t fd = file descriptor
 *         char* buf = buffer to copy into
 *         int32_t bytes = number of bytes to copy into buf
 * OUTPUTS: FAIL, for FAIL. Otherwise, 0 for PASS
 * SIDE EFFECTS:
 */
int32_t read(int32_t fd, void* buf, int32_t nbytes) {
	// Get pcb
	pcb_t* cur_pcb = get_pcb();
	// Check if fd is within range
	if (fd < 0 || fd >= MAX_NUM_FILES)
		return FAIL;
	// Check if it's writing to stdin
	if (fd == 1)
		return FAIL;
	/* If the current file is NOT in use, then open has not been called
	 * and HENCE it needs to return failed
	 */
	if (!cur_pcb->files[fd].in_use)
		return FAIL;
	// Check if a read function exists for this file, and return 0 if not
	if (cur_pcb->files[fd].fd_table->read == NULL)
		return 0;
	// Return appropriate read function
	return ((cur_pcb->files[fd]).fd_table)->read(fd, buf, nbytes);
}

/*
 * SYSTEM CALL that then refers to the appropriate fd then invokes it
 * INPUTS: int32_t fd = file descriptor
 *         const void* buf = buffer to copy into
 *         int32_t bytes = number of bytes to copy into buf
 * OUTPUTS: FAIL, for FAIL. Otherwise, 0 for PASS
 * SIDE EFFECTS:
 */
int32_t write(int32_t fd, const void* buf, int32_t nbytes) {
	// Get pcb
	pcb_t* cur_pcb = get_pcb();
	// Check if fd is within range
	if (fd < 0 || fd >= MAX_NUM_FILES)
		return FAIL;
	// Check if it's writing to stdin
	if (fd == 0)
		return FAIL;
	/* If the current file is NOT in use, then open has not been called
	 * and HENCE it needs to return failed
	 */
	if (!cur_pcb->files[fd].in_use)
		return FAIL;
	// Check if a write function exists for this file, and return 0 if not
	if (cur_pcb->files[fd].fd_table->write == NULL)
		return 0;
	// Return appropriate read function
	return ((cur_pcb->files[fd]).fd_table)->write(fd, buf, nbytes);
}

/*
 * SYSTEM CALL that then refers to the appropriate fd then invokes it
 * INPUTS: const uint8_t* filename = the file being loaded
 *
 * OUTPUTS: FAIL, for FAIL. Otherwise, 0 for PASS
 * SIDE EFFECTS: Occupies a spot in FD table
 */
int32_t open(const uint8_t* filename) {
	pcb_t* cur_pcb = get_pcb();
	
	// Check through fd table to see if it's full
	uint8_t i = 0;
	for (i = 0; i < MAX_NUM_FILES; i++) {
		// If a fd table has free position, BREAK and use that
		if (!cur_pcb->files[i].in_use) {
			break;
		}
	}
	
	// If fd table has no free position, return FAIL
	if (i == MAX_NUM_FILES)
		return FAIL;
	
	// Create a new dentry to help access type of file
	dentry_t dentry;
	if (read_dentry_by_name(filename, &dentry) == FAIL)
		return FAIL;

	// Now use a switch case on type of filename
	switch (dentry.filetype) {
		case RTC_FILE:
			cur_pcb->files[i].in_use = 1;
			cur_pcb->files[i].file_pos = 0;
			cur_pcb->files[i].inode = 0;
			cur_pcb->files[i].fd_table = &(rtc_table);
			break;
		case DIRECTORY:
			cur_pcb->files[i].in_use = 1;
			cur_pcb->files[i].file_pos = 0;
			cur_pcb->files[i].fd_table = &(dir_table);
			break;
		case REG_FILE:
			cur_pcb->files[i].in_use = 1;
			cur_pcb->files[i].file_pos = 0;
			cur_pcb->files[i].inode = dentry.inode; // Use an inode
			cur_pcb->files[i].fd_table = &(file_table);
			break;
		default:
			return FAIL;
	}

	// Run the appropriate open function, and if it fails, return FAIL
	if (cur_pcb->files[i].fd_table->open() == FAIL)
		return FAIL;
	
	// Otherwise, return the position used in the fd table
	return i;
}

/*
 * SYSTEM CALL that then refers to appropriate fd and then closes it
 * INPUTS: int32_t fd = file descriptor
 *
 * OUTPUTS: FAIL, for FAIL. Otherwise, 0 for PASS
 * SIDE EFFECTS: Occupies a spot in FD table
 */
int32_t close(int32_t fd) {
	pcb_t* cur_pcb = get_pcb();

	// If it is trying to close stdin/stdout, FAIL
	if(fd == 0 || fd == 1)
		return FAIL;
	// Check if fd is within range
	if (fd < 0 || fd >= MAX_NUM_FILES)
		return FAIL;

	/* If the current file is NOT in use, then open has not been called
	 *  so it needs to return failed
	 */
	if (!cur_pcb->files[fd].in_use)
		return FAIL;

	// Check if a close function exists for this file and use it if so
	if (cur_pcb->files[fd].fd_table->close != NULL)
		cur_pcb->files[fd].fd_table->close();

	// Clear through everything in the current file descriptor table
	cur_pcb->files[fd].in_use = 0;
	cur_pcb->files[fd].file_pos = 0;
	cur_pcb->files[fd].inode = 0;
	cur_pcb->files[fd].fd_table = NULL;
	return PASS;
}

/*
 * SYSTEM CALL that copies the arguments into the given buf
 * INPUTS: 	uint8_t* buf = userspace buffer asking for the argument
 * 			int32_t nbytes = he number of bytes to copy over
 * OUTPUTS: FAIL, for FAIL. Otherwise, 0 for PASS
 * SIDE EFFECTS: N/A
 */
int32_t getargs(uint8_t* buf, int32_t nbytes) {
	// Get pcb
	pcb_t* cur_pcb = get_pcb();

	// Needed for loop
	int i;

	// Copy into buf with the number of bytes requested
	for (i = 0; i < nbytes && cur_pcb->args[i] != '\0'; i++) {
		buf[i] = cur_pcb->args[i];
	}

	// If there is space in buf, add null
	for(; i < nbytes; i++) {
		buf[i] = '\0';
	}

	//strncpy(buf,(uint8_t *)cur_pcb->args,nbytes);
	return PASS;
}

/*
 * SYSTEM CALL maps video memory for user-level programs
 * INPUTS: 	uint8_t **screen_start = where to copy video memory into
 *
 * OUTPUTS: FAIL, for FAIL. Otherwise, 0 for PASS
 * SIDE EFFECTS: N/A
 */
int32_t vidmap(uint8_t **screen_start) {
	// Check that the pointer lies within the page allocated to this process
	// Since all processes have their memory mapped to 128MB, this amounts to a check for
	//  whether or not the pointer is in the range 128MB - 132MB
	if ((uint32_t)screen_start < EXECUTABLE_VIRT_PAGE_START ||
		(uint32_t)screen_start >= EXECUTABLE_VIRT_PAGE_START + LARGE_PAGE_SIZE)
		return -1;

	int32_t retval = map_video_mem_user((void**)screen_start);

	// Copy the value into the PCB if the mapping succeeded
	if (retval == 0)
		get_pcb()->vid_mem = *screen_start;

	// Return whether or not it succeeded
	return retval;
}

/*
 * SYSTEM CALL that is currently unimplemented
 * INPUTS: 	int32_t signum = parameter
 *			void* handler_address = parameter
 * OUTPUTS: FAIL, for FAIL. Otherwise, 0 for PASS
 * SIDE EFFECTS: N/A
 */
int32_t set_handler(int32_t signum, void* handler_address) {
	return FAIL;
}

/*
 * SYSTEM CALL that is currently unimplemented
 * INPUTS: 	N/A
 * OUTPUTS: FAIL, for FAIL. Otherwise, 0 for PASS
 * SIDE EFFECTS: N/A
 */
int32_t sigreturn(void) {
	return FAIL;
}
