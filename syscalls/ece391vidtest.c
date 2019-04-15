#include <stdint.h>

#include "ece391support.h"
#include "ece391syscall.h"

#define NUM_COLS    80
#define NUM_ROWS    25

int main() {
	uint8_t *video_mem = 0;

	if (ece391_vidmap(&video_mem) == -1) {
		ece391_fdputs(1, (uint8_t*)"We failed bois hehe\n");
	}

	int i;
	for (i = 0; i < NUM_COLS * NUM_ROWS; i++) {
		video_mem[i * 2] = '&';
		video_mem[i * 2 + 1] = 0x3;
    }

    return 0;
}
