#ifndef _PROCESSES_H
#define _PROCESSES_H

#include "types.h"

// Magic number that must appear in the first 4 bytes of all executables
#define ELF_MAGIC 0x464C457F
// The virtual address that all processes' data is mapped to
#define EXECUTABLE_VIRT_PAGE_START 0x8000000
// The offset within a page that an executable should be copied to
#define EXECUTABLE_PAGE_OFFSET 0x48000
// The offset in the executable where the entrypoint of the program is stored
#define ENTRYPOINT_OFFSET 24

// The maximum number of processes that can be running at the same time
#define MAX_NUM_PROCESSES 6
// The maximum number of files that can be open for a process
#define MAX_NUM_FILES 8

typedef struct file_t {
	//Stores pointer to open/close/read/write
	int32_t* fd_table;

	// // Returns -1 if could not open the file and 0 otherwise
	// int32_t (*open)(const uint8_t* filename);
	// // Returns -1 on failure
	// int32_t (*close)(int32_t fd);
	// // Returns the number of bytes read
	// int32_t (*read)(int32_t fd, void *buf, int32_t bytes);
	// // Returns the number of bytes written or -1 on failure
	// int32_t (*write)(int32_t fd, void *buf, int32_t bytes);


	// The inode number of the file (only valid for data file, should be 0 for directories)
	uint32_t inode;
	// The current position in the file
	uint32_t file_pos;
	// Set to 1 if this file array entry is in use and 0 if not
	uint32_t in_use;
} file_t;

typedef struct pcb_t {
	// An array of the files that are being used by the process
	file_t files[MAX_NUM_FILES];
	// The PID of the process
	int32_t pid;
} pcb_t;

// Starts the process associated with the given shell command
int32_t start_process(const char *command);

#endif
