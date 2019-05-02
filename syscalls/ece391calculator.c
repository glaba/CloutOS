#include <stdint.h>

#include "ece391support.h"
#include "ece391syscall.h"

#define BUFSIZE 6
// FUnction declerations
void draw_pixel(uint32_t *screenBase, uint32_t screenWidth, uint32_t x, uint32_t y, uint32_t color);
void draw_thick_line_vertical(uint32_t *screenBase, uint32_t screenWidth, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t width, uint32_t color);
void draw_thick_line_horizontal(uint32_t *screenBase, uint32_t screenWidth, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t width, uint32_t color);
void draw_line(uint32_t *screenBase, uint32_t screenWidth, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t color);
void draw_rect(uint32_t *screenBase, uint32_t screenWidth, uint32_t left, uint32_t top, uint32_t right, uint32_t bottom, uint32_t width, uint32_t color);
void fill_rect(uint32_t *screenBase, uint32_t screenWidth, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void fill_circle(uint32_t *screenBase, uint32_t screenWidth, uint32_t x0, uint32_t y0, uint32_t radius, uint32_t color);
void put_string(uint32_t *screen_base, uint32_t screenWidth, unsigned char *c, uint32_t x, uint32_t y, int32_t color);
void put_char(uint32_t *screen_base, uint32_t screenWidth, unsigned char c, uint32_t x, uint32_t y, uint32_t foreground_color);
void draw_to_display(uint32_t *screen_base, uint32_t screenWidth, int ans);
char button_pressed(uint8_t x, uint8_t y);
int char_to_int(char c);

#define sgn(x) ((x < 0) ? -1 : ((x > 0) ? 1 : 0)) /* macro to return the sign of a number */
#define abs(n) ((n < 0) ? (-n) : (n))             /* macro ro return the abs value of a number */
#define max(x, y) (((x) > (y)) ? (x) : (y))

// 32 byte header size, 14 * 8 bytes per glyph, 512 glyphs
#define FONT_SIZE 32 + 14 * 512

int fd;
int window_id;
uint32_t *canvas_buffer;
uint32_t alloc_buffer[BUFSIZE];
int8_t font_data[FONT_SIZE];

void draw_pixel(uint32_t *screenBase, uint32_t screenWidth, uint32_t x, uint32_t y, uint32_t color) {
    screenBase[screenWidth * y + x] = color;
}

void draw_thick_line_vertical(uint32_t *screenBase, uint32_t screenWidth, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t width, uint32_t color) {
    int i;
    for (i = 0; i < width; i++)
        draw_line(screenBase, screenWidth, x1 + i, y1, x2 + i, y2, color);
}

void draw_thick_line_horizontal(uint32_t *screenBase, uint32_t screenWidth, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t width, uint32_t color) {
    int i;
    for (i = 0; i < width; i++)
        draw_line(screenBase, screenWidth, x1, y1 + i, x2, y2 + i, color);
}

void draw_line(uint32_t *screenBase, uint32_t screenWidth, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t color) {
    int i, dx, dy, sdx, sdy, dxabs, dyabs, x, y, px, py;

    dx = x2 - x1;      /* the horizontal distance of the line */
    dy = y2 - y1;      /* the vertical distance of the line */
    dxabs = abs(dx);
    dyabs = abs(dy);
    sdx = sgn(dx);
    sdy = sgn(dy);
    x = dyabs >> 1;
    y = dxabs >> 1;
    px = x1;
    py = y1;

    draw_pixel(screenBase, screenWidth, px, py, color);

    if (dxabs >= dyabs) {
        for(i=0; i < dxabs; i++) {
            y += dyabs;
            if (y >= dxabs) {
                y -= dxabs;
                py += sdy;
            }
            px += sdx;
            draw_pixel(screenBase, screenWidth, px, py, color);
        }
    }
    else {
        for(i = 0; i < dyabs; i++) {
            x += dxabs;
            if (x >= dyabs) {
                x -= dyabs;
                px += sdx;
            }
            py += sdy;
            draw_pixel(screenBase, screenWidth, px, py, color);    
        }
    }
}

void draw_rect(uint32_t *screenBase, uint32_t screenWidth, uint32_t left, uint32_t top, uint32_t right, uint32_t bottom, uint32_t width, uint32_t color) {
    int32_t top_offset, bottom_offset, i, j, temp;
    if (top > bottom) {
        temp = top;
        top = bottom;
        bottom = temp;
    }
    if (left > right) {
        temp = left;
        left = right;
        right = temp;
    }
    for (j = 0; j < width; j++) {
        // REMEMBER THE LEFT SHIFT 10 IS SPECIFICALLY FOR 1024 width
        top_offset = ((top + j) << 10); 
        bottom_offset = ((bottom + j) << 10); 
        for(i = left; i < right + width; i++) {
            screenBase[top_offset + i] = color;
            screenBase[bottom_offset + i] = color;
        }
    }
    for (j = 0; j < width; j++) {
        for(i = top_offset; i <= bottom_offset; i += screenWidth) {
            screenBase[left + i + j] = color;
            screenBase[right + i + j] = color;
        }
    }
}

void fill_rect(uint32_t *screenBase, uint32_t screenWidth, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    int i, j;
    for (i = x; i < x + width; i++) {
        for (j = y; j < y + height; j++) {
            draw_pixel(screenBase, screenWidth, i, j, color);
        }
    }
}

void fill_circle(uint32_t *screenBase, uint32_t screenWidth, uint32_t x0, uint32_t y0, uint32_t radius, uint32_t color) {
    int32_t x = radius;
    int32_t y = 0;
    int32_t xChange = 1 - (radius << 1);
    int32_t yChange = 0;
    int32_t radiusError = 0;
    int i;

    while (x >= y)     {
        for (i = x0 - x; i <= x0 + x; i++) {
            draw_pixel(screenBase, screenWidth, i, y0 + y, color);
            draw_pixel(screenBase, screenWidth, i, y0 - y, color);
        }
        for (i = x0 - y; i <= x0 + y; i++) {
            draw_pixel(screenBase, screenWidth, i, y0 + x, color);
            draw_pixel(screenBase, screenWidth, i, y0 - x, color);
        }

        y++;
        radiusError += yChange;
        yChange += 2;
        if (((radiusError << 1) + xChange) > 0) {
            x--;
            radiusError += xChange;
            xChange += 2;
        }
    }
}


void put_string(uint32_t *screen_base, uint32_t screenWidth, unsigned char *c, uint32_t x, uint32_t y, int32_t color) {
    int8_t *buf = (int8_t*)c;
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

void draw_to_display(uint32_t *screen_base, uint32_t screenWidth, int ans) {
    uint8_t buffer[10];
    ece391_itoa(ans, buffer, 10);
    put_string(screen_base, screenWidth, buffer, 50, 92, 0x00000000);
    ece391_update_window(window_id);
}


int main () {
    
    alloc_buffer[0] = 1024 / 2;   //x
    alloc_buffer[1] = 200;    //y
    alloc_buffer[2] = 200;   //width
    alloc_buffer[3] = 400;   //height
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

    fill_rect(canvas_buffer, alloc_buffer[2], 0, 15, 200, 400, 0xFFFFFFFF);
    
    draw_thick_line_horizontal(canvas_buffer, alloc_buffer[2], 0, 200, alloc_buffer[2], 200, 5, 0x006200ee);

    draw_thick_line_vertical(canvas_buffer, alloc_buffer[2], 0, 200, 0, 400, 5, 0x006200ee);
    draw_thick_line_vertical(canvas_buffer, alloc_buffer[2], 46, 200, 46, 400, 5, 0x006200ee);
    draw_thick_line_vertical(canvas_buffer, alloc_buffer[2], 97, 200, 97, 400, 5, 0x006200ee);
    draw_thick_line_vertical(canvas_buffer, alloc_buffer[2], 148, 200, 148, 400, 5, 0x006200ee);
    draw_thick_line_vertical(canvas_buffer, alloc_buffer[2], 195, 200, 195, 400, 5, 0x006200ee);
    
    draw_thick_line_horizontal(canvas_buffer, alloc_buffer[2], 0, 245, alloc_buffer[2], 245, 5, 0x006200ee);
    draw_thick_line_horizontal(canvas_buffer, alloc_buffer[2], 0, 245, alloc_buffer[2], 245, 5, 0x006200ee);
    draw_thick_line_horizontal(canvas_buffer, alloc_buffer[2], 0, 295, alloc_buffer[2], 295, 5, 0x006200ee);
    draw_thick_line_horizontal(canvas_buffer, alloc_buffer[2], 0, 345, alloc_buffer[2], 345, 5, 0x006200ee);
    draw_thick_line_horizontal(canvas_buffer, alloc_buffer[2], 0, 395, alloc_buffer[2], 395, 5, 0x006200ee);
    
    put_string(canvas_buffer, alloc_buffer[2], (unsigned char *)"X", 15, 213, 0x00000000);
    put_string(canvas_buffer, alloc_buffer[2], (unsigned char *)"/", 62, 213, 0x00000000);
    put_string(canvas_buffer, alloc_buffer[2], (unsigned char *)"+", 112, 213, 0x00000000);
    put_string(canvas_buffer, alloc_buffer[2], (unsigned char *)"C", 159, 213, 0x00000000);

    put_string(canvas_buffer, alloc_buffer[2], (unsigned char *)"1", 15, 260, 0x00000000);
    put_string(canvas_buffer, alloc_buffer[2], (unsigned char *)"2", 62, 260, 0x00000000);
    put_string(canvas_buffer, alloc_buffer[2], (unsigned char *)"3", 112, 260, 0x00000000);
    put_string(canvas_buffer, alloc_buffer[2], (unsigned char *)"-", 159, 260, 0x00000000);

    put_string(canvas_buffer, alloc_buffer[2], (unsigned char *)"4", 15, 310, 0x00000000);
    put_string(canvas_buffer, alloc_buffer[2], (unsigned char *)"5", 62, 310, 0x00000000);
    put_string(canvas_buffer, alloc_buffer[2], (unsigned char *)"6", 112, 310, 0x00000000);
    put_string(canvas_buffer, alloc_buffer[2], (unsigned char *)"=", 159, 310, 0x00000000);

    put_string(canvas_buffer, alloc_buffer[2], (unsigned char *)"7", 15, 360, 0x00000000);
    put_string(canvas_buffer, alloc_buffer[2], (unsigned char *)"8", 62, 360, 0x00000000);
    put_string(canvas_buffer, alloc_buffer[2], (unsigned char *)"9", 112, 360, 0x00000000);
    put_string(canvas_buffer, alloc_buffer[2], (unsigned char *)"0", 159, 360, 0x00000000);

    draw_to_display(canvas_buffer, alloc_buffer[2], 0);

    ece391_update_window(window_id);
    ece391_fdputs (1, (uint8_t*)"Ran the window program\n");

    // int first_number = 0;
    // int second_number = 0;
    // char operation;
    // int cycle = -1;
    while (1) {
        uint8_t mouse_buffer[5];
        char out_buf[10];

        int num_read = ece391_read(2, mouse_buffer, 5);

        if (num_read > 0) {
            draw_pixel(canvas_buffer, alloc_buffer[2], mouse_buffer[1], mouse_buffer[2], 0xFFFFFFFF);
            uint8_t x = mouse_buffer[1];
            uint8_t y = mouse_buffer[2];

            if (x > 0 && x < 50 && y > 200 && y < 250){
                fill_rect(canvas_buffer, alloc_buffer[2], 0, 15, 200, 160, 0xFFFFFFFF);
                put_string(canvas_buffer, alloc_buffer[2], "X", 50, 92, 0x00000000);
            }
            if (x > 50 && x <= 100 && y >= 200 && y <= 250) {
                fill_rect(canvas_buffer, alloc_buffer[2], 0, 15, 200, 160, 0xFFFFFFFF);
                put_string(canvas_buffer, alloc_buffer[2], "/", 50, 92, 0x00000000);
            }
            if (x > 100 && x <= 150 && y >= 200 && y <= 250) {
                fill_rect(canvas_buffer, alloc_buffer[2], 0, 15, 200, 160, 0xFFFFFFFF);
                put_string(canvas_buffer, alloc_buffer[2], "+", 50, 92, 0x00000000);
            }
            if (x > 150 && x <= 200 && y >= 200 && y <= 250) {
                fill_rect(canvas_buffer, alloc_buffer[2], 0, 15, 200, 160, 0xFFFFFFFF);
            }
            // Second row
            if (x >= 0 && x <= 50 && y > 250 && y <= 300) {
                fill_rect(canvas_buffer, alloc_buffer[2], 0, 15, 200, 160, 0xFFFFFFFF);
                put_string(canvas_buffer, alloc_buffer[2], "1", 50, 92, 0x00000000);
            }
            if (x > 50 && x <= 100 && y > 250 && y <= 300) {
                fill_rect(canvas_buffer, alloc_buffer[2], 0, 15, 200, 160, 0xFFFFFFFF);
                put_string(canvas_buffer, alloc_buffer[2], "2", 50, 92, 0x00000000);
            }
            if (x > 100 && x <= 150 && y > 250 && y <= 300) {
                fill_rect(canvas_buffer, alloc_buffer[2], 0, 15, 200, 160, 0xFFFFFFFF);
                put_string(canvas_buffer, alloc_buffer[2], "3", 50, 92, 0x00000000);
            }
            if (x > 150 && x <= 200 && y > 250 && y <= 300) {
                fill_rect(canvas_buffer, alloc_buffer[2], 0, 15, 200, 160, 0xFFFFFFFF);
                put_string(canvas_buffer, alloc_buffer[2], "i", 50, 92, 0x00000000);
            }
            // Third row
            if (x >= 0 && x <= 50 && y > 300 && y <= 350) {
                fill_rect(canvas_buffer, alloc_buffer[2], 0, 15, 200, 160, 0xFFFFFFFF);
                put_string(canvas_buffer, alloc_buffer[2], "4", 50, 92, 0x00000000);
            }
            if (x > 50 && x <= 100 && y > 300 && y <= 350) {
                fill_rect(canvas_buffer, alloc_buffer[2], 0, 15, 200, 160, 0xFFFFFFFF);
                put_string(canvas_buffer, alloc_buffer[2], "5", 50, 92, 0x00000000);
            }
            if (x > 100 && x <= 150 && y > 300 && y <= 350) {
                fill_rect(canvas_buffer, alloc_buffer[2], 0, 15, 200, 160, 0xFFFFFFFF);
                put_string(canvas_buffer, alloc_buffer[2], "6", 50, 92, 0x00000000);
            }
            if (x > 150 && x <= 200 && y > 300 && y <= 350) {
                fill_rect(canvas_buffer, alloc_buffer[2], 0, 15, 200, 160, 0xFFFFFFFF);
                put_string(canvas_buffer, alloc_buffer[2], "=", 50, 92, 0x00000000);
            }
            // Fourth row
            if (x >= 0 && x <= 50 && y > 350 && y <= 400) {
                fill_rect(canvas_buffer, alloc_buffer[2], 0, 15, 200, 160, 0xFFFFFFFF);
                put_string(canvas_buffer, alloc_buffer[2], "7", 50, 92, 0x00000000);
            }
            if (x > 50 && x <= 100 && y > 350 && y <= 400) {
                fill_rect(canvas_buffer, alloc_buffer[2], 0, 15, 200, 160, 0xFFFFFFFF);
                put_string(canvas_buffer, alloc_buffer[2], "8", 50, 92, 0x00000000);
            }
            if (x > 100 && x <= 150 && y > 350 && y <= 400) {
                fill_rect(canvas_buffer, alloc_buffer[2], 0, 15, 200, 160, 0xFFFFFFFF);
                put_string(canvas_buffer, alloc_buffer[2], "9", 50, 92, 0x00000000);
            }
            if (x > 150 && x <= 200 && y > 350 && y <= 400) {
                fill_rect(canvas_buffer, alloc_buffer[2], 0, 15, 200, 160, 0xFFFFFFFF);
                put_string(canvas_buffer, alloc_buffer[2], "0", 50, 92, 0x00000000);
            }
            ece391_update_window(window_id);
        }
        // uint8_t mouse_buffer[5];  // 0 - window id, 1 - relative x, 2 - relative y, 3 - left click, 4 - right click
        // int num_read;
        // if (ece391_read(2, mouse_buffer, 5) > 0) {
        //     draw_pixel(canvas_buffer, alloc_buffer[2], mouse_buffer[1], mouse_buffer[2], 0x00000000);
        //     // first_number = char_to_int(button_pressed(mouse_buffer));
        //     put_string(canvas_buffer, alloc_buffer[2], button_pressed(mouse_buffer[1], mouse_buffer[2]), 50, 92, 0x00000000);
        //     // draw_to_display(canvas_buffer, alloc_buffer[2], first_number);
        //     ece391_update_window(window_id);
        //     cycle++;
        //     // continue;
        // }
        // switch (cycle) {
        //     case 0:
        //         first_number = char_to_int(button_pressed(mouse_buffer));
        //         draw_to_display(canvas_buffer, alloc_buffer[2], first_number);
        // //         cycle++;
        //         break;
        //     case 1:
        //         operation = button_pressed(mouse_buffer);
        //         put_string(canvas_buffer, alloc_buffer[2], operation, 50, 92, 0x00000000);
        // //         cycle++;
        //         break;
        //     case 2:
        //         second_number = char_to_int(button_pressed(mouse_buffer));
        //         draw_to_display(canvas_buffer, alloc_buffer[2], first_number);
        //         cycle = -1;
        //         switch (operation)
        //         {
        //             case '+':
        //                 draw_to_display(canvas_buffer, alloc_buffer[2], first_number + second_number);
        //                 break;
        //             default:
        //                 break;
        //         }
        //         break;
        //     default:
        //         cycle = -1;
        //         break;
        // }

    }

    return 0;
}

// char button_pressed(uint8_t x, uint8_t y) {
//     // int x = buffer[1];
//     // int y = buffer[2];

//     // First row
//     if (x >= 0 && x <= 50 && y >= 200 && y <= 250) {
//         return "X";
//     }
//     if (x > 50 && x <= 100 && y >= 200 && y <= 250) {
//         return "/";
//     }
//     if (x > 100 && x <= 150 && y >= 200 && y <= 250) {
//         return "+";
//     }
//     if (x > 150 && x <= 200 && y >= 200 && y <= 250) {
//         return "C";
//     }
//     // Second row
//     if (x >= 0 && x <= 50 && y > 250 && y <= 300) {
//         return "1";
//     }
//     if (x > 50 && x <= 100 && y > 250 && y <= 300) {
//         return "2";
//     }
//     if (x > 100 && x <= 150 && y > 250 && y <= 300) {
//         return "3";
//     }
//     if (x > 150 && x <= 200 && y > 250 && y <= 300) {
//         return "-";
//     }
//     // Third row
//     if (x >= 0 && x <= 50 && y > 300 && y <= 350) {
//         return "4";
//     }
//     if (x > 50 && x <= 100 && y > 300 && y <= 350) {
//         return "5";
//     }
//     if (x > 100 && x <= 150 && y > 300 && y <= 350) {
//         return "6";
//     }
//     if (x > 150 && x <= 200 && y > 300 && y <= 350) {
//         return "=";
//     }
//     // Fourth row
//     if (x >= 0 && x <= 50 && y > 350 && y <= 400) {
//         return "7";
//     }
//     if (x > 50 && x <= 100 && y > 350 && y <= 400) {
//         return "8";
//     }
//     if (x > 100 && x <= 150 && y > 350 && y <= 400) {
//         return "9";
//     }
//     if (x > 150 && x <= 200 && y > 350 && y <= 400) {
//         return "0";
//     }
//     return "";
// }

int char_to_int(char c) {
    switch (c) {
        case '1':
            return 1;
            break;
        case '2':
            return 2;
            break;
        case '3':
            return 3;
            break;
        case '4':
            return 4;
            break;
        case '5':
            return 5;
            break;
        case '6':
            return 6;
            break;
        case '7':
            return 7;
            break;
        case '8':
            return 8;
            break;
        case '9':
            return 9;
            break;
        case '0':
            return 0;
            break;
        default:
            return -1;
    }
}

