#include "system_calls.h"
#include "processes.h"
#include "keyboard.h"
#include "paging.h"
#include "kheap.h"
#include "window_manager/window_manager.h"


// Instead of return -1 or 0, used labels/macros
#define PASS 0
#define FAIL -1

/* Jump table for specific read/write/open/close functions */
static struct fops_t rtc_table = {.open = &rtc_open, .close = &rtc_close, .read = &rtc_read, .write = &rtc_write};
static struct fops_t file_table = {.open = &file_open, .close = &file_close, .read = &file_read, .write = &file_write};
static struct fops_t dir_table = {.open = &dir_open, .close = &dir_close, .read = &dir_read, .write = &dir_write};

/*
 * Sets the return value of a system call by setting the new value of EAX after the kernel returns
 *  back to userspace
 *
 * INPUTS: value: the value to return
 */
void syscall_set_retval(uint32_t value) {
	get_user_context()->eax = value;
}

/*
 * A generic system call interface that the assembly linkage calls
 */ 
void sys_call(uint32_t syscall_number, uint32_t param1, uint32_t param2, uint32_t param3) {
	switch (syscall_number) {
		case 1: 
			syscall_set_retval(halt(param1)); 
			break;
		case 2: 
			syscall_set_retval(execute((const char*)param1)); 
			break;
		case 3: 
			syscall_set_retval(read((int32_t)param1, (void*)param2, (int32_t)param3)); 
			break;
		case 4: 
			syscall_set_retval(write((int32_t)param1, (const void*)param2, (int32_t)param3)); 
			break;
		case 5: 
			syscall_set_retval(open((const uint8_t*)param1)); 
			break;
		case 6: 
			syscall_set_retval(close((int32_t)param1)); 
			break;
		case 7: 
			syscall_set_retval(getargs((uint8_t*)param1, (int32_t)param2)); 
			break;
		case 8: 
			syscall_set_retval(FAIL);
			break;
		case 9: 
			syscall_set_retval(set_handler((int32_t)param1, (void*)param2)); 
			break;
		case 10: 
			syscall_set_retval(sigreturn()); 
			break;
		case 11:
			syscall_set_retval(allocate_window((int32_t)param1, (uint32_t*)param2));
			break;
		case 12: 
			syscall_set_retval(update_window((int32_t)param1));
			break;
		default: 
			syscall_set_retval(FAIL);
			break;
	}
}

/*
 * System call that halts the currently running process with the specified status
 * INPUTS: status: a status code between 0 and 256 that indicates how the program exited
 * OUTPUTS: FAIL if halting the process failed, and PASS otherwise
 */
int32_t halt(uint32_t status) {
	SYSCALL_DEBUG("Begin halt system call\n");

	return process_halt(status & 0xFF);
}

/*
 * System call that executes the given shell command
 * INPUTS: command: a shell command
 * OUTPUTS: the status which with the program exits on completion
 */
int32_t execute(const char *command) {
	SYSCALL_DEBUG("Begin execute system call\n");

	spin_lock_irqsave(pcb_spin_lock);

	// Get pcb
	pcb_t *cur_pcb = get_pcb();

	// Check if the command is a valid string
	if (is_userspace_string_valid((void*)command, cur_pcb->pid) == -1) {
		spin_unlock_irqsave(pcb_spin_lock);
		return FAIL;
	}

	spin_unlock_irqsave(pcb_spin_lock);
	return process_execute(command, 1, cur_pcb->tty, 1);
}

/*
 * System call that reads from the file specified by the file descriptor into the provided buffer
 * INPUTS: fd: file descriptor
 *         buf: buffer to copy into
 *         bytes: number of bytes to copy into buf
 * OUTPUTS: -1 on failure, and the number of bytes copied on success
 */
int32_t read(int32_t fd, void *buf, int32_t nbytes) {
	SYSCALL_DEBUG("Begin read system call\n");

	// Check if it's reading from stdout
	if (fd == STDOUT)
		return FAIL;

	spin_lock_irqsave(pcb_spin_lock);

	// Get pcb
	pcb_t *cur_pcb = get_pcb();

	// Check if fd is within range
	if (fd < 0 || fd >= cur_pcb->files.length) {
		spin_unlock_irqsave(pcb_spin_lock);
		return FAIL;
	}

	// Check that the buffer is valid
	if (is_userspace_region_valid(buf, nbytes, cur_pcb->pid) == -1) {
		spin_unlock_irqsave(pcb_spin_lock);
		return FAIL;
	}

	// If the current file is not in use, then open has not been called
	//  so we cannot read from it
	if (!cur_pcb->files.data[fd].in_use) {
		spin_unlock_irqsave(pcb_spin_lock);
		return FAIL;
	}

	// If no read function exists, we say 0 bytes were read
	if (cur_pcb->files.data[fd].fd_table->read == NULL) {
		spin_unlock_irqsave(pcb_spin_lock);
		return 0;
	}

	// Call the appropriate read function
	int32_t retval = cur_pcb->files.data[fd].fd_table->read(fd, buf, nbytes);

	spin_unlock_irqsave(pcb_spin_lock);
	return retval;
}

/*
 * System call that writes the data in the given buffer to the specified file
 * INPUTS: fd: file descriptor
 *         buf: buffer to copy from
 *         bytes: number of bytes written
 * OUTPUTS: -1 on failure, and the number of bytes written otherwise
 */
int32_t write(int32_t fd, const void *buf, int32_t nbytes) {
	SYSCALL_DEBUG("Begin write system call\n");

	// Check if it's writing to stdin
	if (fd == STDIN)
		return FAIL;

	spin_lock_irqsave(pcb_spin_lock);
	
	// Get pcb
	pcb_t *cur_pcb = get_pcb();

	// Check if fd is within range
	if (fd < 0 || fd >= cur_pcb->files.length) {
		spin_unlock_irqsave(pcb_spin_lock);
		return FAIL;
	}

	// Check that the buffer is valid
	if (is_userspace_region_valid((void*)buf, nbytes, cur_pcb->pid) == -1) {
		spin_unlock_irqsave(pcb_spin_lock);
		return FAIL;
	}
	
	// Check if the file has been opened
	if (!cur_pcb->files.data[fd].in_use) {
		spin_unlock_irqsave(pcb_spin_lock);
		return FAIL;
	}

	// Check if a write function exists for this file, and return 0 bytes written if not
	if (cur_pcb->files.data[fd].fd_table->write == NULL) {
		spin_unlock_irqsave(pcb_spin_lock);
		return 0;
	}

	// Call the appropriate write function
	int32_t retval = cur_pcb->files.data[fd].fd_table->write(fd, buf, nbytes);

	spin_unlock_irqsave(pcb_spin_lock);
	return retval;
}

/*
 * System call that opens the file with the given filename
 *
 * INPUTS: filename: the name of the file to open
 * OUTPUTS: -1 on failure, and a non-negative file descriptor on success
 * SIDE EFFECTS: uses up a slot in the fd table
 */
int32_t open(const uint8_t* filename) {
	SYSCALL_DEBUG("Begin open system call\n");

	spin_lock_irqsave(pcb_spin_lock);

	pcb_t *cur_pcb = get_pcb();

	// Check that the filename is a valid string
	if (is_userspace_string_valid((void*)filename, cur_pcb->pid) == -1) {
		spin_unlock_irqsave(pcb_spin_lock);
		return FAIL;
	}

	// Check if this process is already at the maximum number of open files
	if (cur_pcb->files.length == MAX_NUM_FILES) {
		spin_unlock_irqsave(pcb_spin_lock);
		return FAIL;
	}

	// Otherwise, continue trying to add the file	
	// Read the dentry corresponding to this file to get the file type
	dentry_t dentry;
	if (read_dentry_by_name(filename, &dentry) == FAIL) {
		spin_unlock_irqsave(pcb_spin_lock);
		return FAIL;
	}

	// Mark the file as in use and starting from an offset of 0
	file_t new_file;
	new_file.in_use = 1;
	new_file.file_pos = 0;
	
	// Add the file to the list of files, and store its index
	int i = DYN_ARR_PUSH(file_t, cur_pcb->files, new_file);
	// If we couldn't add the file to the table, return failure
	if (i < 0) {
		spin_unlock_irqsave(pcb_spin_lock);
		return FAIL;
	}

	// Implement different behavior depending on the file type
	switch (dentry.filetype) {
		case RTC_FILE:
			cur_pcb->files.data[i].fd_table = &(rtc_table);
			break;
		case DIRECTORY:
			cur_pcb->files.data[i].fd_table = &(dir_table);
			break;
		case REG_FILE:
			// For a file on disk, we need to keep track of the inode as well
			cur_pcb->files.data[i].inode = dentry.inode; 
			cur_pcb->files.data[i].fd_table = &(file_table);
			break;
		default:
			// Remove the file from the list of files
			DYN_ARR_POP(file_t, cur_pcb->files);
			spin_unlock_irqsave(pcb_spin_lock);
			return FAIL;
	}

	// Run the appropriate open function, and return failure if it fails
	if (cur_pcb->files.data[i].fd_table->open(filename) == FAIL) {
		// Remove the file from the list of files
		DYN_ARR_POP(file_t, cur_pcb->files);
		spin_unlock_irqsave(pcb_spin_lock);
		return FAIL;
	}
	
	// Otherwise, return the file descriptor
	spin_unlock_irqsave(pcb_spin_lock);
	return i;
}

/*
 * System call that closes the file associated with the given file descriptor
 *
 * INPUTS: fd: the file descriptor for the file to be closed
 * OUTPUTS: -1 on failure and 0 on success
 * SIDE EFFECTS: clears a spot in the fd table
 */
int32_t close(int32_t fd) {
	SYSCALL_DEBUG("Begin close system call\n");

	// If it is trying to close stdin/stdout, FAIL
	if (fd == STDIN || fd == STDOUT)
		return FAIL;

	spin_lock_irqsave(pcb_spin_lock);
	
	pcb_t *cur_pcb = get_pcb();

	// Check if fd is within range
	if (fd < 0 || fd >= cur_pcb->files.length) {
		spin_unlock_irqsave(pcb_spin_lock);
		return FAIL;
	}

	// Check that the fd is actually in use (we can't close an unopened file)
	if (!cur_pcb->files.data[fd].in_use) {
		spin_unlock_irqsave(pcb_spin_lock);
		return FAIL;
	}

	// Check if a close function exists for this file type and use it if so
	if (cur_pcb->files.data[fd].fd_table->close != NULL)
		cur_pcb->files.data[fd].fd_table->close(fd);

	// Mark the file as not in use
	cur_pcb->files.data[fd].in_use = 0;

	// Remove elements from the end of the dynamic array if possible (which preserves existing FDs)
	int i;
	for (i = cur_pcb->files.length - 1; i >= 0; i--) {
		// If this is in use, no elements with lower index can be removed, since that will
		//  change the index of this in the array, changing the FD
		if (cur_pcb->files.data[i].in_use)
			break;

		// Otherwise, pop the end off the array
		DYN_ARR_POP(file_t, cur_pcb->files);
	}

	spin_unlock_irqsave(pcb_spin_lock);
	return PASS;
}

/*
 * System call that copies the arguments for the current program into the given buffer
 *
 * INPUTS: buf: userspace buffer to copy the arguments into
 *         nbytes: the number of bytes to copy
 * OUTPUTS: -1 on failure and 0 on success
 */
int32_t getargs(uint8_t* buf, int32_t nbytes) {
	SYSCALL_DEBUG("Begin getargs system call\n");

	spin_lock_irqsave(pcb_spin_lock);

	// Get pcb
	pcb_t *cur_pcb = get_pcb();

	// Return failure if there are no arguments
	if (cur_pcb->args[0] == '\0') {
		spin_unlock_irqsave(pcb_spin_lock);
		return FAIL;
	}

	// Check that the buffer is valid
	if (is_userspace_region_valid(buf, nbytes, cur_pcb->pid) == -1) {
		spin_unlock_irqsave(pcb_spin_lock);
		return FAIL;
	}

	// Get the length of the actual arguments
	uint32_t len;
	for (len = 0; cur_pcb->args[len] != '\0'; len++) {}
	// Increment the length by 1 to account for the null termination
	len++;

	// Check that the string will fit within the buffer
	if (len > nbytes) {
		spin_unlock_irqsave(pcb_spin_lock);
		return FAIL;	
	}

	// Copy the arguments into the buffer
	int i;
	for (i = 0; i < nbytes && cur_pcb->args[i] != '\0'; i++) {
		buf[i] = cur_pcb->args[i];
	}
	// Null terminate the string
	buf[i] = NULL;

	spin_unlock_irqsave(pcb_spin_lock);
	return PASS;
}

/*
 * Sets the signal handler for the provided signal number
 */
int32_t set_handler(int32_t signum, void *handler_address) {
	SYSCALL_DEBUG("Begin set_handler system call\n");

	// Check that the signal number is valid
	if (signum < 0 || signum >= NUM_SIGNALS)
		return FAIL;

	spin_lock_irqsave(pcb_spin_lock);

	pcb_t *cur_pcb = get_pcb();

	// Check if the program wants to clear the handler to the default action
	if (handler_address == NULL) {
		cur_pcb->signal_handlers[signum] = NULL;
		spin_unlock_irqsave(pcb_spin_lock);
		return PASS;
	}

	// Check that the handler address is valid
	if (is_userspace_region_valid(handler_address, 1, get_pid()) == -1) {
		spin_unlock_irqsave(pcb_spin_lock);
		return FAIL;
	}

	// Finally, set the actual handler
	cur_pcb->signal_handlers[signum] = (signal_handler)handler_address;

	spin_unlock_irqsave(pcb_spin_lock);
	return PASS;
}

/*
 * Cleans up the userspace stack after a signal handler was called
 */
int32_t sigreturn() {
	SYSCALL_DEBUG("Begin sigreturn system call\n");

	// Cleanup any modifications to the userspace program by handle_signals and
	//  restore the userspace stack / registers back to their previous values
	cleanup_signal();

	// Set the current value of EAX as the return value, since we don't want to overwrite it
	return get_user_context()->eax;
}

/*
 * System call that maps video memory for user-level programs
 *
 * OUTPUTS: screen_start: stores the virtual memory address that video memory is paged into
 * RETURNS: -1 on failure and 0 on success
 * SIDE EFFECTS: pages in video memory
 */
int32_t allocate_window(int32_t fd, uint32_t *buf) {
	// printf("ALLOCATE WINDOW\n");
	uint32_t* buffer;
	if ((buffer = alloc_window(buf[0], buf[1], buf[2], buf[3], &buf[4], get_pid())) == NULL)	
		return FAIL;
	buf[5] = (uint32_t)buffer;
	return PASS;
}

/*
 * System call that instructs the OS to redraw the window of given ID
 */
int32_t update_window(int32_t id) {
	// printf("UPDATE WINDOW ID: %d\n", id);
	if (redraw_window(id) == -1)
		return FAIL;
	return PASS;
}
