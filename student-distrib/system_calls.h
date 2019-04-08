#ifndef _SYSTEM_CALLS_H
#define _SYSTEM_CALLS_H

#include "types.h"
#include "rtc.h"
#include "file_system.h"
#include "keyboard.h"
#include "processes.h"


#define SIZE_OF_JUMP_TABLE 4

/* The ten system calls */
int32_t halt(uint8_t status);
int32_t execute(const char* command);
int32_t read(int32_t fd, void* buf, int32_t nbytes);
int32_t write(int32_t fd, const void* buf, int32_t nbytes);
int32_t open(const uint8_t* filename);
int32_t close(int32_t fd);
int32_t getargs(uint8_t* buf, int32_t nbytes);
int32_t vidmap(uint8_t** screen_start);
int32_t set_handler(int32_t signum, void* handler_address);
int32_t sigreturn(void);








// MACROS to specify type of name
#define RTC_FILE 0
#define DIRECTORY 1
#define REG_FILE 2

//MACROS to specify where in table to point to
#define OPEN 0
#define CLOSE 1



#endif
