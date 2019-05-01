#ifndef _VMWARE_SVGA
#define _VMWARE_SVGA

#include "../pci.h"
#include "../types.h"
#include "../lib.h"
#include "../paging.h"
#include "../keyboard.h"
#include "graphics.h"
#include "../mouse.h"

// #define SVGA_DEBUG_ENABLE

#ifdef SVGA_DEBUG_ENABLE
	#define SVGA_DEBUG(f, ...) printf(f, ##__VA_ARGS__)
#else
	#define SVGA_DEBUG(f, ...) // Nothing
#endif

#define SVGA_PANIC(message, ...) {				        \
	svga_disable();						    	        \
	printf(message, ##__VA_ARGS__);                     \
}


#define SYSTEM_RESOLUTION_WIDTH             1024
#define SYSTEM_RESOLUTION_HEIGHT            768
#define SYSTEM_COLOR_DEPTH                  32
#define BYTES_PER_PIXEL                     4

extern pci_driver svga_driver;

// Describes card registers
enum {
   SVGA_REG_ID = 0,
   SVGA_REG_ENABLE = 1,
   SVGA_REG_WIDTH = 2,
   SVGA_REG_HEIGHT = 3,
   SVGA_REG_MAX_WIDTH = 4,
   SVGA_REG_MAX_HEIGHT = 5,
   SVGA_REG_DEPTH = 6,
   SVGA_REG_BITS_PER_PIXEL = 7,       /* Current bpp in the guest */
   SVGA_REG_PSEUDOCOLOR = 8,
   SVGA_REG_RED_MASK = 9,
   SVGA_REG_GREEN_MASK = 10,
   SVGA_REG_BLUE_MASK = 11,
   SVGA_REG_BYTES_PER_LINE = 12,
   SVGA_REG_FB_START = 13,            /* (Deprecated) */
   SVGA_REG_FB_OFFSET = 14,
   SVGA_REG_VRAM_SIZE = 15,
   SVGA_REG_FB_SIZE = 16,

   /* ID 0 implementation only had the above registers, then the palette */

   SVGA_REG_CAPABILITIES = 17,
   SVGA_REG_MEM_START = 18,           /* (Deprecated) */
   SVGA_REG_MEM_SIZE = 19,
   SVGA_REG_CONFIG_DONE = 20,         /* Set when memory area configured */
   SVGA_REG_SYNC = 21,                /* See "FIFO Synchronization Registers" */
   SVGA_REG_BUSY = 22,                /* See "FIFO Synchronization Registers" */
   SVGA_REG_GUEST_ID = 23,            /* Set guest OS identifier */
   SVGA_REG_CURSOR_ID = 24,           /* (Deprecated) */
   SVGA_REG_CURSOR_X = 25,            /* (Deprecated) */
   SVGA_REG_CURSOR_Y = 26,            /* (Deprecated) */
   SVGA_REG_CURSOR_ON = 27,           /* (Deprecated) */
   SVGA_REG_HOST_BITS_PER_PIXEL = 28, /* (Deprecated) */
   SVGA_REG_SCRATCH_SIZE = 29,        /* Number of scratch registers */
   SVGA_REG_MEM_REGS = 30,            /* Number of FIFO registers */
   SVGA_REG_NUM_DISPLAYS = 31,        /* (Deprecated) */
   SVGA_REG_PITCHLOCK = 32,           /* Fixed pitch for all modes */
   SVGA_REG_IRQMASK = 33,             /* Interrupt mask */

   /* Legacy multi-monitor support */
   SVGA_REG_NUM_GUEST_DISPLAYS = 34,/* Number of guest displays in X/Y direction */
   SVGA_REG_DISPLAY_ID = 35,        /* Display ID for the following display attributes */
   SVGA_REG_DISPLAY_IS_PRIMARY = 36,/* Whether this is a primary display */
   SVGA_REG_DISPLAY_POSITION_X = 37,/* The display position x */
   SVGA_REG_DISPLAY_POSITION_Y = 38,/* The display position y */
   SVGA_REG_DISPLAY_WIDTH = 39,     /* The display's width */
   SVGA_REG_DISPLAY_HEIGHT = 40,    /* The display's height */

   /* See "Guest memory regions" below. */
   SVGA_REG_GMR_ID = 41,
   SVGA_REG_GMR_DESCRIPTOR = 42,
   SVGA_REG_GMR_MAX_IDS = 43,
   SVGA_REG_GMR_MAX_DESCRIPTOR_LENGTH = 44,

   SVGA_REG_TRACES = 45,            /* Enable trace-based updates even when FIFO is on */
   SVGA_REG_GMRS_MAX_PAGES = 46,    /* Maximum number of 4KB pages for all GMRs */
   SVGA_REG_MEMORY_SIZE = 47,       /* Total dedicated device memory excluding FIFO */
   SVGA_REG_TOP = 48,               /* Must be 1 more than the last register */

   SVGA_PALETTE_BASE = 1024,        /* Base of SVGA color map */
   /* Next 768 (== 256*3) registers exist for colormap */

//    SVGA_SCRATCH_BASE = SVGA_PALETTE_BASE + SVGA_NUM_PALETTE_REGS
//                                     /* Base of scratch registers */
//    /* Next reg[SVGA_REG_SCRATCH_SIZE] registers exist for scratch usage:
//       First 4 are reserved for VESA BIOS Extension; any remaining are for
//       the use of the current SVGA driver. */
};


typedef struct SVGADevice {
    uint32_t *fifo_buffer;
    uint32_t *frame_buffer;
    uint32_t fifo_size;
    uint32_t frame_buffer_size;
    int32_t device_id;
    int32_t capabilities;

    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pitch;
    uint32_t depth;
    uint32_t vram_size;

    struct {
        uint32_t x1;
        uint32_t y1;
        uint32_t x2;
        uint32_t y2;
    } offscreen;

    struct {
        uint32_t  reserved_size;
        uint32_t  next_fence;
    } fifo;

} SVGADevice;

extern SVGADevice svga;

/*
 * SVGA_CMD_UPDATE --
 *
 *    This is a DMA transfer which copies from the Guest Framebuffer
 *    (GFB) at BAR1 + SVGA_REG_FB_OFFSET to any screens which
 *    intersect with the provided virtual rectangle.
 * Availability:
 *    Always available.
 */
typedef struct {
	uint32_t x;
	uint32_t y;
	uint32_t width;
	uint32_t height;
} svga_FIFO_cmd_update;

/*
 * SVGA_CMD_RECT_COPY --
 *
 *    Perform a rectangular DMA transfer from one area of the GFB to
 *    another, and copy the result to any screens which intersect it.
 *
 * Availability:
 *    SVGA_CAP_RECT_COPY
 */
typedef struct {
	uint32_t src_x;
	uint32_t src_y;
	uint32_t dest_x;
	uint32_t dest_y;
	uint32_t width;
	uint32_t height;
} svga_FIFO_cmd_rect_copy;

/*
 * SVGA_CMD_FENCE --
 *
 *    Insert a synchronization fence.  When the SVGA device reaches
 *    this command, it will copy the 'fence' value into the
 *    SVGA_FIFO_FENCE register. It will also compare the fence against
 *    SVGA_FIFO_FENCE_GOAL. If the fence matches the goal and the
 *    SVGA_IRQFLAG_FENCE_GOAL interrupt is enabled, the device will
 *    raise this interrupt.
 *
 * Availability:
 *    SVGA_FIFO_FENCE for this command,
 *    SVGA_CAP_IRQMASK for SVGA_FIFO_FENCE_GOAL.
 */
typedef struct {
	uint32_t fence;
} svga_FIFO_cmd_fence;

/*
 * SVGA_CMD_ESCAPE --
 *
 *    Send an extended or vendor-specific variable length command.
 *    This is used for video overlay, third party plugins, and
 *    internal debugging tools. See svga_escape.h
 *
 * Availability:
 *    SVGA_FIFO_CAP_ESCAPE
 */
typedef struct {
	uint32_t nsid;
	uint32_t size;
   /* followed by 'size' bytes of data */
} svga_FIFO_cmd_escape;

// Initializes the registers of the Esvga device
int32_t svga_init_device(pci_function *func);

void svga_enable();

int32_t svga_map_memory(); 

void svga_disable();

void svga_setmode(uint32_t width, uint32_t height, uint32_t bpp);

void svga_out(uint16_t index, uint32_t value);

int32_t svga_in(uint16_t index);

void svga_FIFO_full();

void* svga_FIFO_reserve(uint32_t bytes);

void svga_FIFO_commit(uint32_t bytes);

void svga_FIFO_commit_all();

void* svga_FIFO_reserve_cmd(uint32_t type, uint32_t bytes);

void svga_update(uint32_t x, uint32_t y, uint32_t width, uint32_t height); 

void svga_rect_copy(uint32_t src_x, uint32_t src_y, uint32_t dest_x, uint32_t dest_y, uint32_t width, uint32_t height);

int svga_has_FIFO_cap(int32_t cap);

#endif



