#include "graphics.h"

#define sgn(x) ((x < 0) ? -1 : ((x > 0) ? 1 : 0)) /* macro to return the sign of a number */
#define abs(n) ((n < 0) ? (-n) : (n))             /* macro ro return the abs value of a number */
#define max(x, y) (((x) > (y)) ? (x) : (y))


#define PSF_FONT_MAGIC 0x864ab572
#define HEADER_SIZE_BYTES 32

// Define some basic colors
#define BLACK  								0x000000FF
#define RED    								0xFF0000FF
#define GREEN  								0x00FF00FF
#define BLUE   								0x0000FFFF
#define GRAY   								0x999999FF
#define WHITE  								0xFFFFFFFF


uint8_t *font_data;

void init_font() {
    font = kmalloc(HEADER_SIZE_BYTES);
    // Gets details about font from header
    printf("FONT HEADER LOADED: %d\n", fs_read("font1.psf", 0, (uint8_t*)font, HEADER_SIZE_BYTES));
    
    // Ensure it is a font file by checking magic number
    if (font->magic != PSF_FONT_MAGIC) {
        printf("ERROR: CANNOT LOAD FONT FILE\n");
        return;
    }
    printf("FONT MAGIC: %x\n", font->magic);
    printf("FONT VERSION: %d\n", font->version);
    printf("FONT HEADER SIZE: %d\n", font->header_size);
    printf("FONT FLAGS: %d\n", font->flags);
    printf("FONT NUM GLYPH: %d\n", font->num_glyph);
    printf("FONT BYTES PER GLPYH: %d\n", font->bytes_per_glyph);
    printf("FONT HEIGHT: %d\n", font->height);
    printf("FONT WIDTH: %d\n", font->width);
    printf("FONT BYTES PER LINE: %d\n", font->bytes_per_glyph / font->height);
    
    // Allocate array for font data
    font_data = kmalloc(font->header_size + font->bytes_per_glyph * font->num_glyph);
    // Reads the whole font file
    printf("FONT FILE LOADED: %d\n", fs_read("font1.psf", 0, font_data, font->header_size + font->bytes_per_glyph * font->num_glyph));

}

void init_vga() {
    svga_enable();
}

void init_graphics() {
    init_font();
    init_vga();
}

void put_string(uint32_t *screen_base, uint32_t screenWidth, unsigned char *c, uint32_t x, uint32_t y, int32_t color) {
    int8_t *buf = c;
    while (*buf != '\0') {
        put_char(screen_base, screenWidth, *buf, x += font->width, y, color);
        buf++;
    }
}

void put_char(uint32_t *screen_base, uint32_t screenWidth, unsigned char c, uint32_t x, uint32_t y, uint32_t foreground_color) {
    uint8_t *glyph = (uint8_t*) (font_data + font->header_size + c * font->bytes_per_glyph);
    int bytes_per_row;
    int row, col;
    int count_bytes;
    int col_count;
    count_bytes = 0;
    col_count = 0;
    bytes_per_row = font->bytes_per_glyph / font->height;
    for (row = 0; row < font->height * bytes_per_row; row++) {
        uint8_t row_mapping = glyph[row];
        for (col = 0; col < 8; col++) {
            if (col_count == font->width - 1) {
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

void draw_pixel(uint32_t *screenBase, uint32_t screenWidth, uint32_t x, uint32_t y, uint32_t color) {
    screenBase[screenWidth * y + x] = color;
}

void draw_pixel_fast(uint32_t *screenBase, uint32_t x, uint32_t y, uint32_t color) {
    // REMEMBER THE LEFT SHIFT 10 IS SPECIFICALLY FOR 1024 width    
    screenBase[(y << 10) + x] = color;
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

void fill_rect(uint32_t *screenBase, uint32_t screenWidth, uint32_t left, uint32_t top, uint32_t right, uint32_t bottom, uint32_t color) {
    int32_t top_offset, bottom_offset, i , temp, width;

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
    // REMEMBER THE LEFT SHIFT 10 IS SPECIFICALLY FOR 1024 width
    top_offset = (top * screenWidth) + left;
    bottom_offset = (bottom * screenWidth) + left;
    width = 4 * (right - left + 1);                     // The 4 is due to each pixel having 4 bytes of info

    for(i = top_offset; i <= bottom_offset; i += screenWidth) {
        memset(&screenBase[i], color, width);
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




