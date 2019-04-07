#ifndef _SYSTEM_CALLS_H
#define _SYSTEM_CALLS_H

#include "types.h"

// Magic number that must appear in the first 4 bytes of all executables
#define ELF_MAGIC 0x464C457F

// The offset within a page that an executable should be copied to
#define EXECUTABLE_PAGE_OFFSET 0x48000

// The offset in the executable where the entrypoint of the program is stored
#define ENTRYPOINT_OFFSET 24

/* The ten system calls */
int32_t halt(uint8_t status);
int32_t execute(const uint8_t* command);
int32_t read(int32_t fd, void* buf, int32_t nbytes);
int32_t write(int32_t fd, const void* buf, int32_t nbytes);
int32_t open(const uint8_t* filename);
int32_t close(int32_t fd);
int32_t getargs(uint8_t* buf, int32_t nbytes);
int32_t vidmap(uint8_t** screen_start);
int32_t set_handler(int32_t signum, void* handler_address);
int32_t sigreturn(void);

#endif
