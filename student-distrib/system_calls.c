#include "system_calls.h"
#include "lib.h"
#include "file_system.h"
#include "paging.h"

static uint32_t cur_pid = 0;

int32_t halt(uint8_t status) {
	return -1;
}

int32_t execute(const uint8_t* command) {
	cli();

	// Get the filename of the executable from the command
	uint8_t name[MAX_FILENAME_LENGTH + 1];
	int i;
	for (i = 0; i < MAX_FILENAME_LENGTH && command[i] != '\0'; i++)
		name[i] = command[i];
	name[i] = '\0';

	// Get the memory address where the executable will be placed and the page that contains it
	void *program_page = (void*)(USER_PROGRAMS_START_ADDR + LARGE_PAGE_SIZE * cur_pid);
	void *program_location = program_page + EXECUTABLE_PAGE_OFFSET;

	// Page in the memory region where the executable will be located so that we can write the executable
	map_region_into_kernel(program_page, LARGE_PAGE_SIZE, 
		PAGE_CACHE | PAGE_READ_WRITE);

	// Load the executable into memory at the address corresponding to the PID
	if (fs_load(name, program_location) != 0)
		return -1;

	// Check that the magic number is present	
	if ((uint32_t*)(program_location) != ELF_MAGIC)
		return -1;

	// Get the entrypoint of the executable (virtual address) from the program data
	void *entrypoint = (void*)(((uint32_t*)program_location)[ENTRYPOINT_OFFSET]);

	
}

int32_t read(int32_t fd, void* buf, int32_t nbytes) {
	return -1;
}

int32_t write(int32_t fd, const void* buf, int32_t nbytes) {
	return -1;
}

int32_t open(const uint8_t* filename) {
	return -1;
}

int32_t close(int32_t fd) {
	return -1;
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
