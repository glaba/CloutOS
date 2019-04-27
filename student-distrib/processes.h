#ifndef _PROCESSES_H
#define _PROCESSES_H

#include "types.h"
#include "system_calls.h"
#include "system_call_linkage.h"
#include "keyboard.h"
#include "dynamic_array.h"
#include "list.h"

// Uncomment PROC_DEBUG_ENABLE to enable debugging
#define PROC_DEBUG_ENABLE
#ifdef PROC_DEBUG_ENABLE
	#define PROC_DEBUG(f, ...) printf(f, ##__VA_ARGS__)
#else
	#define PROC_DEBUG(f, ...) // Nothing
#endif

// The number of text-based TTYs (1-based indices)
#define NUM_TEXT_TTYS 3

// Magic number that must appear in the first 4 bytes of all executables
#define ELF_MAGIC 0x464C457F
// The virtual address that all processes' data is mapped to
#define EXECUTABLE_VIRT_PAGE_START 0x8000000
// The offset within a page that an executable should be copied to
#define EXECUTABLE_PAGE_OFFSET 0x48000
// The offset in the executable where the entrypoint of the program is stored
#define ENTRYPOINT_OFFSET 24

// Bitmask for ESP that will yield the top of the kernel stack
#define KERNEL_STACK_BASE_BITMASK 0xFFFFE000

// The maximum number of processes that can be running at the same time
#define MAX_NUM_PROCESSES 6
// The maximum number of files that can be open for a process
// This limit only exists to prevent userspace programs from using up too much kernel memory
#define MAX_NUM_FILES 8

// The static file descriptors assigned to stdin and stdout for all programs
#define STDIN  0
#define STDOUT 1

typedef struct fops_t {
	int32_t (*open )(const uint8_t*);
	int32_t (*close)(int32_t);
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

// A dynamic array containing file_t elements
typedef DYNAMIC_ARRAY(file_t, file_dyn_arr) file_dyn_arr;

// Represents a page mapping
typedef struct page_mapping {
	// The index of the virtual address of the page (corresponding to 4MB jumps per index)
	int virt_index;
	// The index of the physical address of the page (corresponding to 4MB jumps per index)
	int phys_index;
} page_mapping;

// A dynamic array of ints where each item represents an index of a 4MB page used by a process
typedef DYNAMIC_ARRAY(page_mapping, page_mapping_dyn_arr) page_mapping_dyn_arr;

typedef struct pcb_t {
	// A dynamic array of the files that are being used by the process
	file_dyn_arr files;
	// A dynamic array of the indices of the 4MB pages allocated to this process (excluding video memory)
	page_mapping_dyn_arr large_page_mappings;
	// The address of the base of the kernel stack
	void *kernel_stack_base;
	// The TTY that this process is in (1-based indices)
	uint8_t tty;
	// The PID of the process; a negative value indicates that this PCB does not represent a valid process
	int32_t pid;
	// The PID of the parent process
	int32_t parent_pid;
	// The buffer of arguments
	int8_t args[TERMINAL_SIZE];
	// The virtual address of paged in video memory
	void *vid_mem;
} pcb_t;

// Initializes any supporting data structures for managing user level processes
int init_processes();
// Starts the process associated with the given shell command
int32_t process_execute(const char *command, uint8_t has_parent, uint8_t tty);
// Halts the current process and returns the provided status code to the parent process
int32_t process_halt(uint16_t status);
// Maps video memory for the current userspace program to either video memory or a buffer depending
//  on whether or not the current program is in the active TTY
int32_t process_vidmap(uint8_t **screen_start);
// Checks if the given region lies within the memory assigned to the process with the given PID
int8_t is_userspace_region_valid(void *ptr, uint32_t size, int32_t pid);
// Checks if the given string lies within the memory assigned to the process with the given PID
int8_t is_userspace_string_valid(void *ptr, int32_t pid);
// Gets the current pcb from the stack
pcb_t* get_pcb();
// Gets the pointer to the start of video memory for the given TTY
void *get_vid_mem(uint8_t tty);
// Switches from the current TTY to the provided TTY
int32_t tty_switch(uint8_t tty);

// The currently active TTY
extern uint8_t active_tty;

#endif
