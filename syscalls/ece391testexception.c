#include <stdint.h>

#include "ece391support.h"
#include "ece391syscall.h"

int main () {
    ece391_fdputs(1, (uint8_t*)"dividing by zero...\n");

    int32_t a = 1 / 0;

    return 0;
}

