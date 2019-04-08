#include "system_calls.h"
#include "processes.h"
#include "keyboard.h"

#define MAX_FILE_TYPE 3
// Used for determining whether something is NULL
#define NULL 0
// Instead of finding out true or false, used labels/macros
#define FALSE 0
#define TRUE 1
// Instead of return -1 or 0, used labels/macros
#define PASS 0
#define FAIL -1

int32_t halt(uint8_t status) {
	return FAIL;
}

int32_t execute(const char *command) {
	return start_process(command);
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
	if (fd < 0 || fd >= MAX_FILE_TYPE)
		return FAIL;
	// Check if cur_pcb is in use
	if (cur_pcb->files[fd].in_use == FALSE)
		return FAIL;
	// Return appropriate read function
	return ((cur_pcb->files[fd]).fd_table)->read(fd,buf,nbytes);
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
	if (fd < 0 || fd >= MAX_FILE_TYPE)
		return FAIL;
	// Check if cur_pcb is in use
	if (!cur_pcb->files[fd].in_use)
		return FAIL;
	// Return appropriate read function
	return ((cur_pcb->files[fd]).fd_table)->write(fd,buf,nbytes);
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
	for(i = 0; i < MAX_NUM_FILES; i++) {
		// If a fd table has free position, BREAK and use that
		if(!cur_pcb->files[i].in_use) {
			break;
		}
	}
	// If fd table has no free position, return FAIL
	if(i == MAX_NUM_FILES)
		return FAIL;
	// Create a new dentry to help access type of file
	dentry_t dentry;
	if(read_dentry_by_name(filename,&dentry) == FAIL)
		return FAIL;
	fops_t fd_table;
	// Now use a switch case on type of filename
	switch(dentry.filetype) {
		case RTC_FILE:
				cur_pcb->files[i].in_use = TRUE;
				cur_pcb->files[i].file_pos = 0;
				cur_pcb->files[i].inode = 0;
				cur_pcb->files[i].fd_table = (int32_t *) (&(rtc_table));
				break;
		case DIRECTORY:
				cur_pcb->files[i].in_use = TRUE;
				cur_pcb->files[i].file_pos = 0;
				cur_pcb->files[i].fd_table = (int32_t *) (&(dir_table));
				break;
		case REG_FILE:
				cur_pcb->files[i].in_use = TRUE;
				cur_pcb->files[i].file_pos = 0;
				cur_pcb->files[i].inode = dentry.inode; // Use an inode
				cur_pcb->files[i].fd_table = (int32_t *) (&(file_table));
				break;
		default:
				return FAIL;
	}
	// Run the appropriate open function, and if it fails, return FAIL
	if(cur_pcb->files[i].type_pointer.open() == FAIL)
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
	// ASSUME get_pcb works
	pcb_t* cur_pcb = get_pcb();
	// Check if fd is within range
	if (fd < 0 || fd >= MAX_FILE_TYPE)
		return FAIL;
	// Check if cur_pcb is in use
	if (!cur_pcb->files[fd].in_use)
		return FAIL;
	/* Clear through everything in the current
	   file descriptor table */
	cur_pcb->files[fd].in_use = 0;
	cur_pcb->files[fd].file_pos = 0;
	cur_pcb->files[fd].inode = 0;
	cur_pcb->files[fd].type_pointer = NULL;
	return 0;
}

int32_t getargs(uint8_t* buf, int32_t nbytes) {
	return FAIL;
}

int32_t vidmap(uint8_t** screen_start) {
	return FAIL;
}

int32_t set_handler(int32_t signum, void* handler_address) {
	return FAIL;
}

int32_t sigreturn(void) {
	return FAIL;
}
