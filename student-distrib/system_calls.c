#include "system_calls.h"

int32_t halt(uint8_t status){
    return -1;
}

int32_t execute(const uint8_t* command){
    // Create different buffers for executable, and arguments

    // Determine size of executable name
    uint32_t size_of_executable = 0;
    while (command[size_of_executable] != ' ')
        size_of_executable++;
    uint8_t executable_in_mem[size_of_executable];

    // Copy over executable file name to char
    for (uint32_t i = 0; i < size_of_executable; i++) {
        executable_in_mem[i] = command[i];
    }

    // Copy over arguments to args_in_mem
    uint32_t size_of_args = size_of_executable + 1;
    while (command[size_of_args] != '\0')
        size_of_args++;
    uint8_t args_in_mem[size_of_args-size_of_executable];

    //
    for (uint32_t i = 0; i <= size_of_args; i++) {
        args_in_mem[i] = command[i];
    }

    // Call the arguments
    getargs(args_in_mem,size_of_args);




}

int32_t read(int32_t fd, void* buf, int32_t nbytes){
    return -1;
}

int32_t write(int32_t fd, const void* buf, int32_t nbytes){
    return -1;
}


int32_t open (const uint8_t* filename){
    return -1;
}

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
>>>>>>> ansh
}
