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
SIDE EFFECTS: none
*/
int32_t open (const uint8_t* filename){
    return -1;
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
