#include "file_system.h"
#include "proc.h"

/* pointer to the bootblock of our filesystem */
static bootblock_t* bootblock;

/* Open, close, read, write system calls for the filesystem */
static int32_t dir_read (int32_t fd, void* buf, int32_t nbytes);
static int32_t fs_read (int32_t fd, void* buf, int32_t nbytes);
static int32_t fs_write (int32_t fd, const void* buf, int32_t nbytes);
static int32_t fs_open (const uint8_t* filename);
static int32_t fs_close (int32_t fd);

/* we use a read only filesystem so fs_write just returns -1 */
int32_t fs_write (int32_t fd, const void* buf, int32_t nbytes){
 return -1;
}

/* opening a file is handeled by the open system call */
int32_t fs_open (const uint8_t* filename){
 return 0;
}

/* closing a file is handeled by the close system call */
int32_t fs_close (int32_t fd){
 return 0;
}

/* file operations for a file */
static fops_t fs_fops = {
	.read = fs_read,
	.write = fs_write,
	.open = fs_open,
	.close = fs_close
};

/* file operations for a directory */
static fops_t dir_fops = {
	.read = dir_read,
	.write = fs_write,
	.open = fs_open,
	.close = fs_close
};

/*
 *	  DESCRIPTION: initialize the filesystem
 *    INPUTS: mem_mod - pointer to a module that specifies the
 *			  memory address of the start of the filesystem.
 *    OUTPUTS: none
 *    RETURN VALUE: none
 */
void fs_init(module_t *mem_mod){
	bootblock = (bootblock_t*)mem_mod->mod_start;

  add_device(DIR_FTYPE, &dir_fops);
	add_device(FILE_FTYPE, &fs_fops);
}

/*
 *	  DESCRIPTION: reads the dentry by filename
 *    INPUTS: fname - filename specifying the file to read from
 *			  dentry - pointer to dentry block to fill
 *    OUTPUTS: none
 *    RETURN VALUE: 0 on success, -1 on failure (nonexistant file)
 *    SIDE EFFECTS: fills the second arg (dentry) with file name,
 * 					file type, and inode number
 */
 int32_t read_dentry_by_name(const uint8_t* fname, dentry_t* dentry){
 	dentry_t* curr_dentry; //iterate through dentries
  int i;
  curr_dentry = &(bootblock->dentry[0]);

 	for(i = 0; i < bootblock->dir_entries_cnt; i++){
 		if((strlen((int8_t*)fname) == strlen((int8_t *) curr_dentry->fname))){
			//we !strcmp because strcmp returns 0 on a match
 			if(!strncmp((int8_t *) fname, (int8_t *) curr_dentry->fname, strlen((int8_t*)curr_dentry->fname))){
 				memcpy(dentry, curr_dentry, BYTES_DENTRY);
 				return 0;
 			}
 		}
 		curr_dentry++;
 	}
 	return -1;
 }

/*
 *	  DESCRIPTION: reads the dentry by index number
 *    INPUTS: index - index specifying file to read from
 *			  dentry - pointer to dentry block to fill
 *    OUTPUTS: none
 *    RETURN VALUE: 0 on success, -1 on failure (invalid index)
 *    SIDE EFFECTS: fills the second arg (dentry) with file name,
 * 					file type, and inode number
 */
 int32_t read_dentry_by_index(uint32_t index, dentry_t* dentry){
 	dentry_t* curr_dentry; //iterate through dentries

 	if(index >= 0 && index < bootblock->dir_entries_cnt){
 		curr_dentry = &(bootblock->dentry[index]);
 		memcpy(dentry, curr_dentry, BYTES_DENTRY);
 		return 0;
 	}
 	return -1;
 }

 /*
 *	  DESCRIPTION: reads 'length' bytes starting from position 'offset'
 * 				   in the file with inode number 'inode'.
 *    INPUTS: inode - inode number specifying file to read from
 			  offset - position to start reading from in the file
 			  buf - buffer to be filled by the bytes read from the file
 			  length - number of bytes to read
 *    OUTPUTS: none
 *    RETURN VALUE: number of bytes read and placed in the buffer
 *    SIDE EFFECTS: fills the third arg (buf) with the bytes read from
 *					the file
 */
 int32_t read_data(uint32_t inode, uint32_t offset, uint8_t* buf, uint32_t length){
 	uint32_t i;
 	int32_t bytes_read = 0;
 	data_block_t* curr_data_block; // data to copy to buf
 	int32_t off_data_block; // data block number in inode
 	int32_t new_offset; // offset once inside correct data block
 	inode_t *curr_inode;
 	bytes_read = 0;

	// bytes_read = read_directory_entry(offset, buf, length);

 	if(inode < 0 || inode >= bootblock->inode_cnt){
 		return bytes_read;
 	}

 	// calculate correct data block and offset to start copying from
 	new_offset = offset % CHARS_PER_BLOCK;
 	off_data_block = offset / CHARS_PER_BLOCK;
 	curr_inode = (inode_t*)((uint8_t*) bootblock + ((inode+ONE) * BLOCK_SIZE));
 	curr_data_block = (data_block_t*) ((uint8_t*) bootblock + (ONE + bootblock->inode_cnt + curr_inode->data_block[off_data_block])*BLOCK_SIZE);

 		// copy data to buf
 	for(i = 0; i < length; i++){
 			//check for EOF
 		if(bytes_read + offset < curr_inode->length){
 			buf[i] = curr_data_block->data[new_offset];
 			bytes_read++;
 			new_offset++;
				// move to next data block
 			if(new_offset >= CHARS_PER_BLOCK){
 				off_data_block++;
 				curr_data_block = (data_block_t*) ((uint8_t*) bootblock + (ONE + bootblock->inode_cnt + curr_inode->data_block[off_data_block])*BLOCK_SIZE);
 				new_offset = 0;
 			}
 		}
 	}
 return bytes_read;
}

 /*
 *	  DESCRIPTION: copies over 'length' bytes of directory entries into 'buf'
 *    INPUTS: buf - buffer to be filled by the bytes read from the file
 			  length - number of bytes to read
 *    OUTPUTS: none
 *    RETURN VALUE: number of bytes read and placed in the buffer
 *    SIDE EFFECTS: fills the first arg (buf) with the bytes read from
 *					the file
 */
 uint32_t read_directory(uint32_t offset, uint8_t* buf, uint32_t length){
 	uint32_t i,k;
 	uint32_t buf_idx = 0;
 	uint32_t buf_off = 0;
 	uint32_t ret_val = 0;
 	dentry_t dentry;

 	for(k = 0; k < bootblock->dir_entries_cnt; k++){
 		read_dentry_by_index(k, &dentry);
 		for(i = 0; i < strlen((int8_t*)dentry.fname); i++){
 			if(buf_off>= offset && ret_val < length){
 				buf[buf_idx++] = dentry.fname[i];
 				ret_val++;
 			}
 			else{
 				buf_off++;
 			}
 		}
 		if(buf_off>= offset && ret_val<length){
 			buf[buf_idx++]= '\n';
 		}
 	}

 	buf[buf_idx] = '\0';
 	return ret_val;
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
 uint32_t read_directory_entry(uint32_t dir_entry, uint8_t* buf, uint32_t length){
 	uint32_t i;
 	uint32_t buf_idx = 0;
 	uint32_t ret_val = 0;
 	dentry_t dentry;

 	if(dir_entry >= bootblock -> dir_entries_cnt || dir_entry < 0) return 0;

 	read_dentry_by_index(dir_entry, &dentry);

 	for(i = 0; i < strlen((int8_t*)dentry.fname); i++){
 		if(ret_val < length){
 			buf[buf_idx++] = dentry.fname[i];
 			ret_val++;
 		}
 	}
 	buf[buf_idx++]= '\n';
 	buf[buf_idx] = '\0';

 	return ret_val;
 }

 /*
 *	  DESCRIPTION: loads contents of the file 'fname' into memory at address 'addr'
 *    INPUTS: fname - filename specifying the file to read from
 			  addr - address to copy contents of file to
 *    OUTPUTS: none
 *    RETURN VALUE: 0 on success, -1 on failure
 *    SIDE EFFECTS: copies contents of the specified file into the memoy
 *					address.
 */
 int32_t load(dentry_t * d, uint8_t * mem) {
 	uint32_t len;
 	inode_t * curr_inode;

 	if(d == NULL || mem == NULL) return -1;

 	curr_inode = (inode_t*)((uint8_t*) bootblock + ((d -> inode+ONE) * BLOCK_SIZE));
 	len = curr_inode -> length;
 	read_data(d -> inode, 0, mem, len);
 	return 0;
 }

/* get_inode_ptr
 *	  DESCRIPTION: get inode pointer given an inode number.
 *    INPUTS: inode - inode number to get the corresponding inode pointer to.
 *    OUTPUTS: none
 *    RETURN VALUE: inode pointer corresponding to the inode number
 */
 inode_t * get_inode_ptr(uint32_t inode) {
 	return (inode_t*) ((uint8_t*) bootblock + (inode + ONE) * BLOCK_SIZE);
 }

/*
 *	  DESCRIPTION: reads directory entries in the filesystem.
 *    INPUTS: fd - file descriptor describing the file to read.
 *			  buf - buffer to will with the bytes read
 *			  nbytes - number of bytes to read
 *    OUTPUTS: fills the 'buf' buffer with the bytes read
 *    RETURN VALUE: number of bytes read
 */
int32_t dir_read (int32_t fd, void* buf, int32_t nbytes){
 	int32_t bytes_read;
 	pcb_t * pcb;
 	fd_t * fs_fd;

 	pcb(pcb);
 	fs_fd = &(pcb -> files[fd]);

 	bytes_read = read_directory_entry(fs_fd -> pos, buf, nbytes);
 	fs_fd -> pos++;

 	return bytes_read;
 }

/*
 *	  DESCRIPTION: reads files in the filesystem.
 *    INPUTS: fd - file descriptor describing the file to read.
 *			  buf - buffer to will with the bytes read
 *			  nbytes - number of bytes to read
 *    OUTPUTS: fills the 'buf' buffer with the bytes read
 *    RETURN VALUE: number of bytes read
 */
 int32_t fs_read (int32_t fd, void* buf, int32_t nbytes){
 	int32_t bytes_read;
 	pcb_t * pcb;
 	fd_t * fs_fd;

 	pcb(pcb);
 	fs_fd = &(pcb -> files[fd]);

 	bytes_read = read_data(fs_fd -> inode_num, fs_fd -> pos, buf, nbytes);
 	fs_fd -> pos += bytes_read;

 	return bytes_read;
 }
