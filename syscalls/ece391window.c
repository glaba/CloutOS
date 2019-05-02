#include <stdint.h>

#include "ece391support.h"
#include "ece391syscall.h"

#define BUFSIZE 6

// 32 byte header size, 14 * 8 bytes per glyph, 512 glyphs
#define FONT_SIZE 32 + 14 * 512

int fd;
int window_id;
uint32_t *canvas_buffer;
uint32_t alloc_buffer[BUFSIZE];
int8_t font_data[FONT_SIZE];

void put_char(uint32_t *screen_base, uint32_t screenWidth, unsigned char c, uint32_t x, uint32_t y, uint32_t foreground_color);


void draw_pixel(uint32_t *screenBase, uint32_t screenWidth, uint32_t x, uint32_t y, uint32_t color) {
    screenBase[screenWidth * y + x] = color;
}

void put_string(uint32_t *screen_base, uint32_t screenWidth, unsigned char *c, uint32_t x, uint32_t y, int32_t color) {
    uint8_t *buf = (uint8_t*)c;
    while (*buf != '\0') {
        put_char(screen_base, screenWidth, *buf, x += 8, y, color);
        buf++;
    }
}

void put_char(uint32_t *screen_base, uint32_t screenWidth, unsigned char c, uint32_t x, uint32_t y, uint32_t foreground_color) {
    uint8_t *glyph = (uint8_t*) (font_data + 32 + c * 14);
    int bytes_per_row;
    int row, col;
    int count_bytes;
    int col_count;
    count_bytes = 0;
    col_count = 0;
    bytes_per_row = 14 / 14;
    for (row = 0; row < 14 * bytes_per_row; row++) {
        uint8_t row_mapping = glyph[row];
        for (col = 0; col < 8; col++) {
            if (col_count == 8 - 1) {
                col_count = 0;
                break;
            }
            if (row_mapping & (int8_t)0x80)
                draw_pixel(screen_base, screenWidth, x + col_count, y, foreground_color);
            
            col_count++;
            row_mapping = (int8_t)row_mapping << 1;
        }
        if ((++count_bytes % bytes_per_row) == 0)
            y++;
    }
}


int main () {
    
    alloc_buffer[0] = 500;   //x
    alloc_buffer[1] = 500;   //y
    alloc_buffer[2] = 200;   //width
    alloc_buffer[3] = 200;   //height
    alloc_buffer[4] = 0;     //id
    alloc_buffer[5] = 0;     //canvas buffer pointer

    if (-1 == ece391_allocate_window(0, alloc_buffer)) {
        ece391_fdputs (1, (uint8_t*)"Cannot allocate window\n");
	    return 1;
    }
    window_id = alloc_buffer[4];
    canvas_buffer = (uint32_t*) alloc_buffer[5];

    if (-1 == (fd = ece391_open ((uint8_t*)"font1.psf"))) {
        ece391_fdputs (1, (uint8_t*)"file not found\n");
	    return 2;
    }

    int cnt;
    cnt = ece391_read (fd, font_data, FONT_SIZE);
    uint8_t cnt_buf[5];
    ece391_itoa(cnt, cnt_buf, 10);
    // Bytes read from the font file
    ece391_fdputs (1, cnt_buf);
    ece391_fdputs (1, (uint8_t*)"\n");

    // Fill canvas with RED
    int i, j;
    for (i = 0; i < alloc_buffer[2]; i++) {
        for (j = 15; j < alloc_buffer[3]; j++) {
            draw_pixel(canvas_buffer, alloc_buffer[2], i, j, 0xFF0000FF);
        }
    }
    put_string(canvas_buffer, alloc_buffer[2], "THIS IS A TEST", 44, 20, 0xFFFFFFFF);
    put_string(canvas_buffer, alloc_buffer[2], "HELLO ECE!", 60, 100, 0xFFFFFFFF);


    ece391_update_window(window_id);
    ece391_fdputs (1, (uint8_t*)"Ran the window program\n");

    while (1);

    return 0;
}

