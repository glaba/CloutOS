#include "system_calls.h"
#include "processes.h"
#include "keyboard.h"

int32_t halt(uint32_t status) {
	return process_halt(status & 0xFF);
}

int32_t execute(const char *command) {
	return process_execute(command, 1);
}

int32_t read(int32_t fd, void* buf, int32_t nbytes) {
	return terminal_read(fd, buf, nbytes);
}

int32_t write(int32_t fd, const void* buf, int32_t nbytes) {
	return terminal_write(fd, buf, nbytes);
}

int32_t open(const uint8_t* filename) {
	return 0;
}

int32_t close(int32_t fd) {
	return 0;
}

int32_t getargs(uint8_t* buf, int32_t nbytes) {
	return -1;
}

int32_t vidmap(uint8_t** screen_start) {
	return -1;
}

int32_t set_handler(int32_t signum, void* handler_address) {
	return -1;
}

int32_t sigreturn(void) {
	return -1;
}
