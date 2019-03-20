#include "file_system.h"
#include "system_calls.h"
#include "x86_desc.h"
#include "lib.h"

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
	if(fs_is_open == 1)
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
	if(fs_is_open == 0)
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
	if(buf == NULL || fname == NULL)
		return -1;

	/* Extract dentry information using the filename passed in. */
	if(read_dentry_by_name((uint8_t *)fname, &dentry) == -1)
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
 *              new process.
 * Inputs: fname- name of file
 *         address- address of read - offset
 * Returns: -1- failure
 *           0- success
 */
int32_t fs_load(const int8_t * fname, uint32_t address){
	/* Local variables. */
	dentry_t dentry;

	/* Check for invalid file name or buffer. */
	if(fname == NULL)
		return -1;

	/* Extract dentry information using the filename passed in. */
	if(read_dentry_by_name((uint8_t *)fname, &dentry) == -1)
		return -1;

	/* Load the entire file at the address passed in. */
	if(read_data(dentry.inode, 0, (uint8_t *)address, inodes[dentry.inode].size)){
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
int32_t read_dentry_by_name(const uint8_t * fname, dentry_t * dentry){
	/* Local variables. */
	int i;

	/* Find the entry in the array. */
	for( i = 0; i < MAX_NUM_FS_DENTRIES; i++ ){
		if(strlen( fs_dentries[i].filename ) == strlen( (int8_t *)fname)){
			if(strncmp(fs_dentries[i].filename, (int8_t *)fname, strlen( (int8_t *)fname)) == 0){
				/* Found it! Copy the data into 'dentry'. */
				strcpy(dentry->filename, fs_dentries[i].filename);
				dentry->filetype = fs_dentries[i].filetype;
				dentry->inode = fs_dentries[i].inode;
				return 0;
			}
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
int32_t read_dentry_by_index(uint32_t index, dentry_t * dentry){
	/* Check for an invalid index. */
	if( index >= MAX_NUM_FS_DENTRIES )
		return -1;

	/* Copy the data into 'dentry'. */
	strcpy( dentry->filename, fs_dentries[index].filename );
	dentry->filetype = fs_dentries[index].filetype;
	dentry->inode = fs_dentries[index].inode;

	return 0;
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
 * -1- failure (bad data block number within inode)
 * 0- end of file has been reached
 * n- number of bytes read and placed in the buffer
 */
int32_t read_data(uint32_t inode, uint32_t offset, uint8_t * buf,
                  uint32_t length){
	/* Local variables. */
	uint32_t  total_successful_reads;
	uint32_t  location_in_block;
	uint32_t  cur_data_block;
	uint8_t * read_addr;

	/* Initializations. */
	total_successful_reads = 0;

	/* Check for an invalid inode number. */
	if( inode >= fs_stats.num_inodes )
		return -1;

	/* Check for invalid offset. */
	if( offset >= inodes[inode].size )
		return 0;

	/* Calculate the starting data block for this read. */
	cur_data_block = offset/FS_PAGE_SIZE;

	/* Check for an invalid data block. */
	if(inodes[inode].data_blocks[cur_data_block] >= fs_stats.num_datablocks)
		return -1;

	/* Calculate the location within the starting data block. */
	location_in_block = offset % FS_PAGE_SIZE;

	/* Calculate the address to start reading from. */
	read_addr = (uint8_t *)(data_start +
				(inodes[inode].data_blocks[cur_data_block])*FS_PAGE_SIZE +
				offset % FS_PAGE_SIZE);

	/* Read all the data. */
	while( total_successful_reads < length ){
		if( location_in_block >= FS_PAGE_SIZE ){
			location_in_block = 0;

			/* Move to the next data block. */
			cur_data_block++;

			/* Check for an invalid data block. */
			if( inodes[inode].data_blocks[cur_data_block] >= fs_stats.num_datablocks ){
				return -1;
			}

			/* Find the start of the next data block. */
			read_addr = (uint8_t *)(data_start + (inodes[inode].data_blocks[cur_data_block])*FS_PAGE_SIZE);
		}

		/* See if we've reached the end of the file. */
		if( total_successful_reads + offset >= inodes[inode].size ){
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
 * Inputs: none
 *
 * Returns: 0- always
 */
int32_t file_open(void){
	return 0;
}

/*
 * Returns: 0- always
 */
int32_t file_close(void){
	return 0;
}

/*
 * Description: Performs a fs_read.
 * Inputs: none
 * Returns:
 * -1- failure (invalid parameters, nonexistent file)
 *  0- success
 */
int32_t file_read( uint8_t * buf, uint32_t length, const int8_t * fname, uint32_t offset ){
	return fs_read(fname, offset, buf, length);
}

/*
 * Inputs: none
 *
 * Returns: -1- always
 */
int32_t file_write(void){
	return -1;
}

/**** Directory operations. ****/

/*
 * Inputs: none
 *
 * Returns: 0- always
 */
int32_t dir_open(void){
	return 0;
}

/*
 * Inputs: none
 *
 * Returns: 0- always
 */
int32_t dir_close(void){
	return 0;
}

/*
 * Description:
 * Implements ls.
 *
 * Inputs: none
 *
 * Returns: n- number of bytes in buf
 */
int32_t dir_read(uint8_t * buf){
	/*
	 * Reset dir_reads and return if we've already read
	 * the whole file system.
	 */
	if(dir_reads >= fs_stats.num_dentries){
		dir_reads = 0;
		return 0;
	}

	/* Copy the next filename into buf. */
	strcpy((int8_t *)buf, (const int8_t *)fs_dentries[dir_reads].filename);

	/* Increment the number of directory reads. */
	dir_reads++;

	/* Return the length of the filename. */
	return strlen((int8_t *)buf);
}

/*
 * Inputs: none
 *
 * Returns: -1- always
 */
int32_t dir_write(void){
	return -1;
}

/***************test cases**********************/

// read_test_text:
// Print out a text file and/or an executable.
// Print out the size in bytes of a text file and/or an executable.
// input: none
// output: none
// effect: print out the content in the specific file, depending on the file flag
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
	if(read_dentry_by_name((uint8_t*)filename, &test_file)==-1){
		printf("failed reading file");
		return;
	}
	read_dentry_by_name((uint8_t*)filename, &test_file);   // read the text file

	printf("The file type the file:%d\n", test_file.filetype);
	printf("The inode index of the file:%d\n", test_file.inode);
	bytes_read = read_data(test_file.inode, 0, buffer, buffer_size);  // size of file
	printf("size of file is : %d btyes\n", (int32_t)bytes_read);

	if (bytes_read <= 0){
		printf("read data failed\n");
		return;
	}

	// print the content of file depending on how large it is
	if (bytes_read>=SIZE_THREAD){
		printf("Since the file is too large,\nwe print the first and last 200 bytes in the file.\n");
		printf("\n");
		printf("First 400 bytes:\n");
		//for (i=0; i<bytes_read; i++)
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


// read_test_exe:
// Print out a text file and/or an executable.
// Print out the size in bytes of a text file and/or an executable.
// input: none
// output: none
// effect: print out the content in the specific file, depending on the file flag
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
	if(read_dentry_by_name((uint8_t*)filename, &test_file)==-1){
		printf("failed reading file");
		return;
	}
	read_dentry_by_name((uint8_t*)filename, &test_file);   // read the execute file

	printf("The file type the file:%d\n", test_file.filetype);
	printf("The inode index of the file:%d\n", test_file.inode);
	bytes_read = read_data(test_file.inode, 0, buffer, buffer_size);  // size of file
	printf("size of file is : %d btyes\n", (int32_t)bytes_read);

	if (bytes_read <= 0){
		printf("read data failed\n");
		return;
	}

	// print the content of file depending on how large it is
	if (bytes_read>=SIZE_THREAD){
		printf("Since the file is too large,\nwe print the first and last 200 bytes in the file.\n");
		printf("\n");
		printf("First 400 bytes:\n");
		//for (i=0; i<bytes_read; i++)
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
