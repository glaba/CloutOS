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

int32_t halt(uint32_t status) {
	return process_halt(status & 0xFF);
}

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
	// Check if cur_pcb is in use
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
	// Check if cur_pcb is in use
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
	// ASSUME get_pcb works
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

	// Check if fd is within range
	if (fd < 0 || fd >= MAX_NUM_FILES)
		return FAIL;

	// Check if cur_pcb is in use
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
	return 0;
}

int32_t getargs(uint8_t* buf, int32_t nbytes) {
	return FAIL;
}

int32_t vidmap(uint8_t **screen_start) {
	int32_t retval = map_video_mem_user((void**)screen_start);
	
	// Copy the value into the PCB if the mapping succeeded
	if (retval == 0)
		get_pcb()->vid_mem = *screen_start;

	// Return whether or not it succeeded
	return retval;
}

int32_t set_handler(int32_t signum, void* handler_address) {
	return FAIL;
}

int32_t sigreturn(void) {
	return FAIL;
}
