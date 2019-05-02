#include <stdint.h>

#include "ece391support.h"
#include "ece391syscall.h"

#define BUFSIZE 6


void draw_pixel(uint32_t *screenBase, uint32_t screenWidth, uint32_t x, uint32_t y, uint32_t color) {
    screenBase[screenWidth * y + x] = color;
}

int main () {
    uint32_t buf[BUFSIZE];
    buf[0] = 500;   //x
    buf[1] = 500;   //y
    buf[2] = 150;    //width
    buf[3] = 150;    //height
    buf[4] = 0;     //id
    buf[5] = 0;     //canvas buffer pointer

    uint32_t buf1[BUFSIZE];
    buf1[0] = 200;   //x
    buf1[1] = 200;   //y
    buf1[2] = 350;   //width
    buf1[3] = 350;   //height
    buf1[4] = 0;     //id
    buf1[5] = 0;     //canvas buffer pointer

    uint32_t buf2[BUFSIZE];		
    buf2[0] = 300;   //x
    buf2[1] = 300;   //y
    buf2[2] = 250;   //width
    buf2[3] = 250;   //height
    buf2[4] = 0;     //id
    buf2[5] = 0;     //canvas buffer pointer


    ece391_allocate_window(0, buf);
    int window_id = buf[4];
    ece391_allocate_window(0, buf1);
    int window_id1 = buf1[4];
    ece391_allocate_window(0, buf2);
    int window_id2 = buf2[4];


    uint32_t *buffer = (uint32_t*) buf1[5];
	uint32_t *buffer1 = (uint32_t*) buf2[5];


    int i, j;
    for (i = 0; i < buf1[2]; i++) {
        for (j = 15; j < buf1[3]; j++) {
            draw_pixel(buffer, buf1[2], i, j, 0xFF0000FF);
        }
    }
    for (i = 0; i < buf2[2]; i++) {
        for (j = 15; j < buf2[3]; j++) {
            draw_pixel(buffer1, buf2[2], i, j, 0xFFFF0000);
        }
    }

    ece391_update_window(window_id);
    ece391_update_window(window_id1);
    ece391_update_window(window_id2);


    ece391_fdputs (1, (uint8_t*)"Ran the window program strxxx 69\n");
    // ece391_fdputs (1, (uint8_t*)"Hi, what's your name? ");
    // if (-1 == (cnt = ece391_allocate_window(0, buf))) {
    //     ece391_fdputs (1, (uint8_t*)"Can't read name from keyboard.\n");
    //     return 3;
    // }
    // buf[cnt] = '\0';
    // ece391_fdputs (1, (uint8_t*)"Hello, ");
    // ece391_fdputs (1, buf);

    return 0;
}

