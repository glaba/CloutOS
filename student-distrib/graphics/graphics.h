#ifndef _GRAPHICS_H
#define _GRAPHICS_H

#include "../types.h"
#include "../lib.h"
#include "../i8259.h"
#include "../kheap.h"
#include "VMwareSVGA.h"


typedef struct {
    uint32_t magic;             /* magic bytes to identify PSF */
    uint32_t version;           /* zero */
    uint32_t header_size;       /* offset of bitmaps in file, 32 */
    uint32_t flags;             /* 0 if there's no unicode table */
    uint32_t num_glyph;         /* number of glyphs */
    uint32_t bytes_per_glyph;   /* size of each glyph */
    uint32_t height;            /* height in pixels */
    uint32_t width;             /* width in pixels */
} PSF_font;

PSF_font *font;


void init_font();

void init_vga();

void init_graphics();

void put_string(uint32_t *screen_base, uint32_t screenWidth, unsigned char *c, uint32_t x, uint32_t y, int32_t color);

void put_char(uint32_t *screen_base, uint32_t screenWidth, unsigned char c, uint32_t x, uint32_t y, uint32_t foreground_color);

void draw_pixel(uint32_t *screenBase, uint32_t screenWidth, uint32_t x, uint32_t y, uint32_t rgbColor);

void draw_pixel_fast(uint32_t *screenBase, uint32_t x, uint32_t y, uint32_t rgbColor);

void draw_thick_line_vertical(uint32_t *screenBase, uint32_t screenWidth, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t width, uint32_t color);

void draw_thick_line_horizontal(uint32_t *screenBase, uint32_t screenWidth, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t width, uint32_t color);

void draw_line(uint32_t *screenBase, uint32_t screenWidth, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t color);

void draw_rect(uint32_t *screenBase, uint32_t screenWidth, uint32_t left, uint32_t top, uint32_t right, uint32_t bottom, uint32_t width, uint32_t color);

void fill_rect(uint32_t *screenBase, uint32_t screenWidth, uint32_t left, uint32_t top, uint32_t right, uint32_t bottom, uint32_t color);

void fill_circle(uint32_t *screenBase, uint32_t screenWidth, uint32_t x0, uint32_t y0, uint32_t radius, uint32_t color);

#endif


