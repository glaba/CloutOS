#ifndef _PROCESSES_H
#define _PROCESSES_H

#include "types.h"
#include "system_calls.h"
#include "system_call_linkage.h"

// Magic number that must appear in the first 4 bytes of all executables
#define ELF_MAGIC 0x464C457F
// The virtual address that all processes' data is mapped to
#define EXECUTABLE_VIRT_PAGE_START 0x8000000
// The offset within a page that an executable should be copied to
#define EXECUTABLE_PAGE_OFFSET 0x48000
// The offset in the executable where the entrypoint of the program is stored
#define ENTRYPOINT_OFFSET 24

// Bitmask for ESP that will yield the base of the kernel stack
#define KERNEL_STACK_BASE_BITMASK 0xFFFFE000

// The maximum number of processes that can be running at the same time
#define MAX_NUM_PROCESSES 6
// The maximum number of files that can be open for a process
#define MAX_NUM_FILES 8

// The static file descriptors assigned to stdin and stdout for all programs
#define STDIN  0
#define STDOUT 1

typedef struct fops_t {
	int32_t (*open )(void);
	int32_t (*close)(void);
	int32_t (*read )(int32_t fd, void *buf, int32_t bytes);
	int32_t (*write)(int32_t fd, const void *buf, int32_t bytes);
} fops_t;

typedef struct file_t {
	// Stores pointer to open/close/read/write
	fops_t* fd_table;
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
	// The PID of the parent process
	int32_t parent_pid;
} pcb_t;

// Starts the process associated with the given shell command
int32_t process_execute(const char *command, uint8_t has_parent);
// Halts the current process and returns the provided status code to the parent process
int32_t process_halt(uint16_t status);

// Gets the current pcb from the stack
pcb_t* get_pcb();

#endif
