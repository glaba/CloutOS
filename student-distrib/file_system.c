#include "file_system.h"
#include "x86_desc.h"
#include "lib.h"
#include "processes.h"


/* Variable to ensure only one 'open' of the file system. */
uint32_t fs_is_open;

/* The statistics for the file system provided by the boot block. */
fs_stats_t fs_stats;

/* The address of the boot block. */
uint32_t bb_start;

/* The address of the first data block. */
uint32_t data_start;

/* The array of directory entries for the file system. */
dentry_t * fs_dentries;

/* The array of inodes. */
inode_t * inodes;

/* The number of filenames read from the filesystem. */
uint32_t dir_reads;

/*
 * Description: Opens the file system by calling fs_init.
 *
 * Inputs: fs_start- location of the boot block
 *         fs_end- end
 * Returns: -1- failure (file sytem already open)
 *           0- success
 */
int32_t fs_open(uint32_t fs_start, uint32_t fs_end){
	/* Return error if the file system is already open. */
	if (fs_is_open == 1)
		return -1;

	/* Initialize the file system. */
	fs_init(fs_start, fs_end);
	fs_is_open = 1;

	return 0;
}

/*
 * Description: Close the file system.
 * Inputs: none
 * Returns: -1- failure (file system already closed)
 *           0- success
 */
int32_t fs_close(void){
	/* Return error if the file system isn't open. */
	if (fs_is_open == 0)
		return -1;

	/* Clear the flag indicating an open file system. */
	fs_is_open = 0;

	return 0;
}

/*
 * Description: Performs a read on the file with name 'fname' by calling read_data
 * for the specified number of bytes and starting at the specified offset
 * in the file.
 * Inputs: fname- name of file
 *         offset- offset to start read
 *         buf- buffer to send to read_data
 *         length- number of bytes
 * Returns: -1- failure (invalid parameters, nonexistent file)
 *           0- success
 */
int32_t fs_read(const int8_t * fname, uint32_t offset, uint8_t * buf,
                uint32_t length){
	/* Local variables. */
	dentry_t dentry;

	/* Check for invalid file name or buffer. */
	if (buf == NULL || fname == NULL)
		return -1;

	/* Extract dentry information using the filename passed in. */
	if (read_dentry_by_name((uint8_t *)fname, &dentry) == -1)
		return -1;

	/* Use read_data to read the file into the buffer passed in. */
	return read_data(dentry.inode, offset, buf, length);
}

/*
 * Description: Does nothing as our file system is read only.
 * Inputs: none
 * Returns: 0- default
 */
int32_t fs_write(void){
	return 0;
}

/*
 * Description: Loads an executable file into memory and prepares to begin the
 *              new process, if the executable file will fit within a 4MB page boundary
 * Inputs: fname- name of file
 *         address- address of read - offset
 * Returns: -1- failure
 *           0- success
 */
int32_t fs_load(const int8_t *fname, void *address) {
	/* Local variables. */
	dentry_t dentry;

	/* Check for invalid file name or buffer. */
	if (fname == NULL)
		return -1;

	/* Extract dentry information using the filename passed in. */
	if (read_dentry_by_name((uint8_t*)fname, &dentry) == -1)
		return -1;

	// Check that the executable will not cross a 4MB page boundary
	if ((inodes[dentry.inode].size + (uint32_t)address) / 0x400000 != (uint32_t)address / 0x400000)
		return -1;

	/* Load the entire file at the address passed in. */
	if (read_data(dentry.inode, 0, (uint8_t*)address, inodes[dentry.inode].size) <= 0) {
		return -1;
	}

	return 0;
}

/*
 * Description: Initializes global variables associated with the file system.
 * Inputs: fs_start- start
 *         fs_end- end
 * Returns: none
 */
void fs_init(uint32_t fs_start, uint32_t fs_end){
	/* Set the location of the boot block. */
	bb_start = fs_start;

	/* Populate the fs_stats variable with the filesystem statistics. */
	memcpy(&fs_stats, (void *)bb_start, FS_STATS_SIZE);

	/* Set the location of the directory entries array. */
	fs_dentries = (dentry_t *)(bb_start + FS_STATS_SIZE);

	/* Set the location of the array of inodes. */
	inodes = (inode_t *)(bb_start + FS_PAGE_SIZE);

	/* Set the location of the first data block. */
	data_start = bb_start + (fs_stats.num_inodes+1)*FS_PAGE_SIZE;

	/* Initialize the number of directory reads to zero. */
	dir_reads = 0;
}

/*
 * Description: Returns directory entry information (file name, file type,
 *              inode number) for the file with the given name via the dentry_t
 *              block passed in.
 * Inputs: fname- name of file
 *         dentry- directory entry
 * Returns: -1- failure (non-existent file)
 *           0- success
 */
int32_t read_dentry_by_name(const uint8_t *fname, dentry_t *dentry) {
	/* Local variables. */
	int i, j;

	// If it's an empty string, return -1
	if (strlen((int8_t *)fname) == 0)
		return -1;

	/* Find the entry in the array. */
	uint32_t fname_len = strlen((int8_t*)fname);
	for (i = 0; i < MAX_NUM_FS_DENTRIES; i++) {
		// Get the length of the current filename without exceeding MAX_FILENAME_LENGTH
		for (j = 0; j < MAX_FILENAME_LENGTH && fs_dentries[i].filename[j] != '\0'; j++);
		uint32_t cur_fname_len = j;

		// Check if the two strings are equal
		// First, check if their lengths are equal, and then if they have the same characters
		uint8_t equal = 0;
		if (fname_len == cur_fname_len) {
			equal = 1;

			for (j = 0; j < fname_len; j++) {
				// If any of the characters are different, they are not equal
				if (fname[j] != fs_dentries[i].filename[j]) {
					equal = 0;
					break;
				}
			}
		}

		// If the requested filename and the current filename are the same
		if (equal) {
			strncpy((int8_t*)dentry->filename, (int8_t*)fs_dentries[i].filename, MAX_FILENAME_LENGTH);
			dentry->filetype = fs_dentries[i].filetype;
			dentry->inode = fs_dentries[i].inode;
			return 0;
		}
	}

	/* If we did not find the file, return failure. */
	return -1;
}

/*
 * Description: Returns directory entry information (file name, file type, inode
 *              number) for the file with the given index via the dentry_t block
 *              passed in.
 * Inputs: fname- name of file
 *         dentry- directory entry
 * Returns: -1- failure (invalid index)
 *           0- success
 */
int32_t read_dentry_by_index(uint32_t index, dentry_t *dentry){
	/* Check for an invalid index. */
	if (index >= MAX_NUM_FS_DENTRIES)
		return -1;

	strncpy((int8_t*)dentry->filename, (int8_t*)fs_dentries[index].filename, MAX_FILENAME_LENGTH);
	//dentry->filename[MAX_FILENAME_LENGTH] = '\0';
	dentry->filetype = fs_dentries[index].filetype;
	dentry->inode = fs_dentries[index].inode;

	return 0;
}

/*
 *	  DESCRIPTION: copies over 'length' bytes of directory entries into 'buf'
 *    INPUTS: entry - directory entry to copy over
 *			  buf - buffer to be filled by the bytes read from the file
 *			  length - number of bytes to read
 *    OUTPUTS: none
 *    RETURN VALUE: number of bytes read and placed in the buffer
 *    SIDE EFFECTS: fills the first arg (buf) with the bytes read from
 *					the file
 */
uint32_t read_directory_entry(uint32_t dir_entry, uint8_t* buf, uint32_t length) {
	uint32_t i;
	uint32_t buf_idx = 0;
	uint32_t ret_val = 0;
	dentry_t dentry;
	// Check if out of bounds
	if (dir_entry >= fs_stats.num_dentries || dir_entry < 0)
		return 0;
	// Read dentry
	read_dentry_by_index(dir_entry, &dentry);
	// Fill in buffer
	for (i = 0; i < strlen((int8_t*)dentry.filename); i++) {
		if (ret_val < length) {
			buf[buf_idx++] = dentry.filename[i];
			ret_val++;
		}
	}
	// Return bytes read
	return ret_val;
}

/*
 * Description:
 * Reads (up to) 'length' bytes starting from position 'offset' in the file
 * with inode number 'inode'. Returns the number of bytes read and placed
 * in the buffer 'buf'.
 *
 * Inputs:
 * inode- index node
 * offset- offset
 * buf- buffer
 * length- number of bytes
 *
 * Returns:
 * -1- failure (bad inode, bad data block)
 * n- number of bytes read and placed in the buffer
 */
int32_t read_data(uint32_t inode, uint32_t offset, uint8_t * buf,
                  uint32_t length) {
	/* Local variables. */
	uint32_t  total_successful_reads;
	uint32_t  location_in_block;
	uint32_t  cur_data_block;
	uint8_t * read_addr;

	/* Initializations. */
	total_successful_reads = 0;

	/* Check for an invalid inode number. */
	if (inode >= fs_stats.num_inodes)
		return -1;

	/* Check for invalid offset. We return 0 bytes read in this case */
	if (offset >= inodes[inode].size)
		return 0;

	/* Calculate the starting data block for this read. */
	cur_data_block = offset / FS_PAGE_SIZE;

	/* Check for an invalid data block. */
	if (inodes[inode].data_blocks[cur_data_block] >= fs_stats.num_datablocks)
		return -1;

	/* Calculate the location within the starting data block. */
	location_in_block = offset % FS_PAGE_SIZE;

	/* Calculate the address to start reading from. */
	read_addr = (uint8_t*)(data_start +
				(inodes[inode].data_blocks[cur_data_block])*FS_PAGE_SIZE +
				offset % FS_PAGE_SIZE);

	/* Read all the data. */
	while (total_successful_reads < length) {
		if (location_in_block >= FS_PAGE_SIZE) {
			location_in_block = 0;

			/* Move to the next data block. */
			cur_data_block++;

			/* Check for an invalid data block. */
			if (inodes[inode].data_blocks[cur_data_block] >= fs_stats.num_datablocks) {
				return -1;
			}

			/* Find the start of the next data block. */
			read_addr = (uint8_t*)(data_start + (inodes[inode].data_blocks[cur_data_block])*FS_PAGE_SIZE);
		}

		/* See if we've reached the end of the file. */
		if ( total_successful_reads + offset >= inodes[inode].size ){
			return total_successful_reads;
		}

		/* Read a byte. */
		buf[total_successful_reads] = *read_addr;
		location_in_block++;
		total_successful_reads++;
		read_addr++;
	}

	return total_successful_reads;
}

/**** Regular file operations. ****/

/*
 * Inputs: filename: not used
 *
 * Returns: 0 always
 */
int32_t file_open(const uint8_t *filename) {
	return 0;
}

/*
 * Returns: 0- always
 */
int32_t file_close(int32_t fd) {
	return 0;
}

/*
 * Description: Performs a fs_read.
 * Inputs: none
 * Returns:
 * -1- failure (invalid parameters, nonexistent file)
 *  0- success
 */
int32_t file_read(int32_t fd, void* buf, int32_t nbytes){
	int32_t bytes_read;
 	pcb_t* pcb;

 	pcb = get_pcb();
	// Read the data from current location in file to buffer
 	bytes_read = read_data(pcb->files.data[fd].inode, pcb->files.data[fd].file_pos, buf, nbytes);
	// Increment file position
 	pcb->files.data[fd].file_pos += bytes_read;

 	return bytes_read;
}

/*
 * Inputs: none
 *
 * Returns: -1- always
 */
int32_t file_write(int32_t fd, const void* buf, int32_t bytes){
	//To ensure parameters have been used
	(void) fd;
	(void) buf;
	(void) bytes;
	return -1;
}

/**** Directory operations. ****/

/*
 * Inputs: none
 *
 * Returns: 0- always
 */
int32_t dir_open(const uint8_t *filename){
	return 0;
}

/*
 * Inputs: none
 *
 * Returns: 0- always
 */
int32_t dir_close(int32_t fd){
	return 0;
}

/*
 * Description: Implements ls.
 * Inputs: none
 * Returns: n- number of bytes in buf
 */
int32_t dir_read(int32_t fd, void* buf, int32_t nbytes){
	int32_t bytes_read;
 	pcb_t* pcb;

 	pcb = get_pcb();
	//load in the directory entry to the buffer
 	bytes_read = read_directory_entry(pcb->files.data[fd].file_pos, buf, nbytes);
	//increment file pos in directory
 	pcb->files.data[fd].file_pos++;

 	return bytes_read;
}

/*
 * Inputs: none
 *
 * Returns: -1- always
 */
int32_t dir_write(int32_t fd, const void* buf, int32_t bytes){
	(void) fd;
	(void) buf;
	(void) bytes;
	return -1;
}

/***************test cases**********************/

/*
 * Print out a text file and/or an executable.
 * Print out the size in bytes of a text file and/or an executable.
 * input: none
 * output: none
 * effect: print out the content in the specific file, depending on the file flag
 */
void read_test_text(uint8_t* filename){
	printf("test reading file...\n");
	clear();
	printf("test reading file...\n");

  printf("Filename: %s\n", filename);

	dentry_t test_file;
	uint32_t i;
	int32_t bytes_read;

	uint32_t buffer_size = SMALL_BUF;
	uint8_t buffer[buffer_size];
	if (read_dentry_by_name((uint8_t*)filename, &test_file)==-1){
		printf("failed reading file");
		return;
	}
	read_dentry_by_name((uint8_t*)filename, &test_file);   // Read the text file

	printf("The file type the file:%d\n", test_file.filetype);
	printf("The inode index of the file:%d\n", test_file.inode);
	bytes_read = read_data(test_file.inode, 0, buffer, buffer_size);  // Size of file
	printf("size of file is : %d btyes\n", (int32_t)bytes_read);

	if (bytes_read <= 0){
		printf("read data failed\n");
		return;
	}

	// Print the content of file depending on how large it is
	if (bytes_read>=SIZE_THREAD){
		printf("Since the file is too large,\nwe print the first and last 200 bytes in the file.\n");
		printf("\n");
		printf("First 400 bytes:\n");
		for (i=0; i<SIZE_THREAD/2; i++){
			printf("%c", buffer[i]);
		}
		printf("\n\n");
		printf("Last 400 bytes:\n");
		for (i=bytes_read-SIZE_THREAD/2; i<bytes_read; i++){
			printf("%c", buffer[i]);
		}
		return;
	}

	for (i=0; i<bytes_read; i++){
		printf("%c", buffer[i]);
	}
	return;
}


/*
 * Print out a text file and/or an executable.
 * Print out the size in bytes of a text file and/or an executable.
 * input: none
 * output: none
 * effect: print out the content in the specific file, depending on the file flag
 */
void read_test_exe(uint8_t* filename){
	printf("test reading file...\n");
	clear();
	printf("test reading file...\n");

  printf("Filename: %s\n", filename);

	dentry_t test_file;
	uint32_t i;
	int32_t bytes_read;


	uint32_t buffer_size = LARGE_BUF;
	uint8_t buffer[buffer_size];
	if (read_dentry_by_name((uint8_t*)filename, &test_file)==-1){
		printf("failed reading file");
		return;
	}
	read_dentry_by_name((uint8_t*)filename, &test_file);   // Read the execute file

	printf("The file type the file:%d\n", test_file.filetype);
	printf("The inode index of the file:%d\n", test_file.inode);
	bytes_read = read_data(test_file.inode, 0, buffer, buffer_size);  // Size of file
	printf("size of file is : %d btyes\n", (int32_t)bytes_read);

	if (bytes_read <= 0){
		printf("read data failed\n");
		return;
	}

	// Print the content of file depending on how large it is
	if (bytes_read>=SIZE_THREAD){
		printf("Since the file is too large,\nwe print the first and last 200 bytes in the file.\n");
		printf("\n");
		printf("First 400 bytes:\n");
		for (i=0; i<SIZE_THREAD/2; i++){
			printf("%c", buffer[i]);
		}
		printf("\n\n");
		printf("Last 400 bytes:\n");
		for (i=bytes_read-SIZE_THREAD/2; i<bytes_read; i++){
			printf("%c", buffer[i]);
		}
		return;
	}

	for (i=0; i<bytes_read; i++){
		printf("%c", buffer[i]);
	}
	return;
}

/***************test cases************************/
