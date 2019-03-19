#include "system_calls.h"

//return -1 for all functions for now will implement later

int32_t halt(uint8_t status){
  return -1;
}
int32_t execute(const uint8_t* command){
  return -1;
}
int32_t read(int32_t fd, void* buf, int32_t nbytes){
  return -1;
}
int32_t write(int32_t fd, const void* buf, int32_t nbytes){
  return -1;
}
/*
DESCRIPTION: sets flags and prepares an unused file descriptor for the named file
INPUTS:
	-const uint8_t* filename: the name of the file to be prepped and put in the pcb
OUTPUTS:
	none
RETURN VALUE:
	-fd the corresponding index in the fd table of the pcb
	- -1 on failure (named file does not exist, or no open file descriptors)
SIDE EFFECTS:
*/
int32_t open (const uint8_t* filename){
    int i, fd = -1;
    pcb_t* pcb;
    dentry_t dentry;
    fd_t* fd_ptr = NULL;
    fops_t * fops;

    pcb(pcb);

    // sti();

    /* find available file descriptor entry */
    /* start after stdin (0) and stdout (1) */
    for(i = 2; i < FILE_ARRAY_LEN ; i++){
        if(!(pcb->files[i].flags && FD_LIVE)){
            fd = i;
            break;
        }
    }

    /* if no available fd directory or valid filename*/
    if(fd == -1 || read_dentry_by_name(filename, &dentry) == -1)
        return -1;

    fd_ptr = &(pcb -> files[i]);
    fops = get_device_fops(dentry.ftype);

    /* RTC or Directory */
    if(dentry.ftype == RTC_FTYPE || dentry.ftype == DIR_FTYPE){
        fd_ptr -> inode = NULL;
        fd_ptr -> inode_num = dentry.inode;
    }

    /* Regular File */
    else if(dentry.ftype == FILE_FTYPE){
        fd_ptr -> inode = get_inode_ptr(dentry.inode);
        fd_ptr -> inode_num = dentry.inode;
    }

    /*setting flags to make the file descriptor live*/
    fd_ptr -> fops = fops;
    fd_ptr -> flags = FD_LIVE;
    fd_ptr -> fops -> open(filename);

    return fd;
}

/*
DESCRIPTION:
	closes the given file descriptor
INPUTS:
	fd - the file descriptor number to close in the pcb
OUTPUTS:none
RETURN VALUE:
	0 - on success
	-1 - on failure (invalid descriptor(stdin or stdout aka 0 or 1, out of bounds)
SIDE EFFECTS: none
*/
int32_t close (int32_t fd){
    pcb_t* pcb;
    fd_t * file_desc;

    /* prevent closing stdin or stdout */
    if(fd < 2 || fd >= FILE_ARRAY_LEN) return -1;

    pcb(pcb);
    file_desc = &(pcb -> files[fd]);

	//clearing flags so the FD is set to unused state
    if(file_desc -> flags && FD_LIVE){

        file_desc -> fops -> close(fd);

        file_desc -> fops = NULL;
        file_desc -> inode = NULL;
        file_desc -> pos = 0;
        file_desc -> flags = 0;
        return 0;
    }

    return -1;
}
int32_t getargs(uint8_t* buf, int32_t nbytes){
  return -1;
}
int32_t vidmap(uint8_t** screen_start){
  return -1;
}
int32_t set_handler(int32_t signum, void* handler_address){
  return -1;
}
int32_t sigreturn(void){
  return -1;
}
