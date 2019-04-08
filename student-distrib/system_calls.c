#include "system_calls.h"
#include "processes.h"
#include "keyboard.h"

#define MAX_FILE_TYPE 3
#define NULL 0
#define TRUE 1

int32_t halt(uint8_t status) {
	return -1;
}

int32_t execute(const char *command) {
	return start_process(command);
}

int32_t read(int32_t fd, void* buf, int32_t nbytes) {
	// Get pcb
	uint32_t cur_pcb = get_pcb();
	// Check if fd is within range
	if (fd < 0 || fd >= MAX_FILE_TYPE)
		return -1;
	// Check if cur_pcb is in use
	if (cur_pcb.files[fd].in_use == 0)
		return -1;
	// Return appropriate read function
	return (cur_pcb.files[fd]).fd_table[READ](fd,buf,nbytes);
}

int32_t write(int32_t fd, const void* buf, int32_t nbytes) {
	// Get pcb
	uint32_t cur_pcb = get_pcb();
	// Check if fd is within range
	if (fd < 0 || fd >= MAX_FILE_TYPE)
		return -1;
	// Check if cur_pcb is in use
	if (cur_pcb.files[fd].in_use == 0)
		return -1;
	// Return appropriate read function
	return (cur_pcb.files[fd]).fd_table[READ](fd,buf,nbytes);
}

int32_t open(const uint8_t* filename) {
	// ASSUME get_pcb works
	uint32_t cur_pcb = get_pcb(whatevs);
	// Check through fd table to see if it's full
	uint8_t i = 0;
	for(i = 0; i < MAX_NUM_FILES; i++) {
		// If a fd table has free position, BREAK and use that
		if(cur_pcb.files[i].in_use == 0) {
			break;
		}
	}
	// If fd table has no free position, return -1
	if(i == MAX_NUM_FILES)
		return -1;
	// Create a new dentry to help access type of file
	dentry_t dentry;
	if(read_dentry_by_name(filename,dentry) == -1)
		return -1;
	fops_t fd_table;
	// Now use a switch case on type of filename
	switch(dentry.filetype) {
		case RTC:
				cur_pcb.files[i].in_use = TRUE;
				cur_pcb.files[i].file_pos = 0;
				cur_pcb.files[i].inode = 0;
				cur_pcb.files[i].type_pointer = &(rtc_table);
				break;
		case DIRECTORY:
				cur_pcb.files[i].in_use = TRUE;
				cur_pcb.files[i].file_pos = 0;
				cur_pcb.files[i].type_pointer = &(dir_table);
				break;
		case REG_FILE:
				cur_pcb.files[i].in_use = TRUE;
				cur_pcb.files[i].file_pos = 0;
				cur_pcb.files[i].inode = dentry.inode;
				cur_pcb.files[i].type_pointer = &(file_table);
				break;
		default:
				return -1;
	}
	if(cur_pcb.files[i].type_pointer[OPEN] == -1)
		return -1;
	return i;
}

int32_t close(int32_t fd) {
	// ASSUME get_pcb works
	uint32_t cur_pcb = get_pcb(whatevs);
	cur_pcb.files[i].in_use = 0;
	cur_pcb.files[i].file_pos = 0;
	cur_pcb.files[i].inode = 0;
	cur_pcb.files[i].type_pointer = NULL;
}

int32_t getargs(uint8_t* buf, int32_t nbytes) {
	return -1;
}

int32_t vidmap(uint8_t** screen_start) {
	return -1;
}

int32_t set_handler(int32_t signum, void* handler_address) {
	return -1;
}

int32_t sigreturn(void) {
	return -1;
}
