#include "VMwareSVGA.h"


// Device specific information
#define VMWARE_VENDOR_ID 					0x15AD
#define VMWARE_DEVICE_ID 					0x405
#define SVGA_INDEX_PORT 					0x0
#define SVGA_VALUE_PORT 					0x1
#define SVGA_VERSION_2     					2
#define SVGA_ID_2          					SVGA_MAKE_ID(SVGA_VERSION_2)
#define SVGA_VERSION_1     					1
#define SVGA_ID_1          					SVGA_MAKE_ID(SVGA_VERSION_1)
#define SVGA_MAGIC         					0x900000
#define SVGA_MAKE_ID(ver)  					(SVGA_MAGIC << 8 | (ver))
#define SVGA_VERSION_0     					0
#define SVGA_ID_0          					SVGA_MAKE_ID(SVGA_VERSION_0)

// More device specific information
#define	PCI_SPACE_MEMORY 					0
#define	PCI_SPACE_IO     					1


#define SVGA_FIFO_CAP_NONE                  0
#define SVGA_FIFO_CAP_FENCE             	(1<<0)
#define SVGA_FIFO_CAP_ACCELFRONT        	(1<<1)
#define SVGA_FIFO_CAP_PITCHLOCK         	(1<<2)
#define SVGA_FIFO_CAP_VIDEO             	(1<<3)
#define SVGA_FIFO_CAP_CURSOR_BYPASS_3   	(1<<4)
#define SVGA_FIFO_CAP_ESCAPE            	(1<<5)
#define SVGA_FIFO_CAP_RESERVE           	(1<<6)
#define SVGA_FIFO_CAP_SCREEN_OBJECT     	(1<<7)
#define SVGA_FIFO_CAP_GMR2              	(1<<8)
#define SVGA_FIFO_CAP_3D_HWVERSION_REVISED  SVGA_FIFO_CAP_GMR2
#define SVGA_FIFO_CAP_SCREEN_OBJECT_2   	(1<<9)
#define SVGA_FIFO_CAP_DEAD              	(1<<10)

#define SVGA_CAP_NONE                       0x00000000
#define SVGA_CAP_RECT_COPY                  0x00000002
#define SVGA_CAP_CURSOR                     0x00000020
#define SVGA_CAP_CURSOR_BYPASS              0x00000040   // Legacy (Use Cursor Bypass 3 instead)
#define SVGA_CAP_CURSOR_BYPASS_2            0x00000080   // Legacy (Use Cursor Bypass 3 instead)
#define SVGA_CAP_8BIT_EMULATION             0x00000100
#define SVGA_CAP_ALPHA_CURSOR               0x00000200
#define SVGA_CAP_3D                         0x00004000
#define SVGA_CAP_EXTENDED_FIFO              0x00008000
#define SVGA_CAP_MULTIMON                   0x00010000   // Legacy multi-monitor support
#define SVGA_CAP_PITCHLOCK                  0x00020000
#define SVGA_CAP_IRQMASK                    0x00040000
#define SVGA_CAP_DISPLAY_TOPOLOGY           0x00080000   // Legacy multi-monitor support
#define SVGA_CAP_GMR                        0x00100000
#define SVGA_CAP_TRACES                     0x00200000
#define SVGA_CAP_GMR2                       0x00400000
#define SVGA_CAP_SCREEN_OBJECT_2            0x00800000


// Respresents the device
SVGADevice svga;


// Describes FIFO commands
enum {
   /*
    * Block 1 (basic registers): The originally defined FIFO registers.
    * These exist and are valid for all versions of the FIFO protocol.
    */

   SVGA_FIFO_MIN = 0,
   SVGA_FIFO_MAX,       /* The distance from MIN to MAX must be at least 10K */
   SVGA_FIFO_NEXT_CMD,
   SVGA_FIFO_STOP,

   /*
    * Block 2 (extended registers): Mandatory registers for the extended
    * FIFO.  These exist if the SVGA caps register includes
    * SVGA_CAP_EXTENDED_FIFO; some of them are valid only if their
    * associated capability bit is enabled.
    *
    * Note that when originally defined, SVGA_CAP_EXTENDED_FIFO implied
    * support only for (FIFO registers) CAPABILITIES, FLAGS, and FENCE.
    * This means that the guest has to test individually (in most cases
    * using FIFO caps) for the presence of registers after this; the VMX
    * can define "extended FIFO" to mean whatever it wants, and currently
    * won't enable it unless there's room for that set and much more.
    */

   SVGA_FIFO_CAPABILITIES = 4,
   SVGA_FIFO_FLAGS,
   // Valid with SVGA_FIFO_CAP_FENCE:
   SVGA_FIFO_FENCE,

   /*
    * Block 3a (optional extended registers): Additional registers for the
    * extended FIFO, whose presence isn't actually implied by
    * SVGA_CAP_EXTENDED_FIFO; these exist if SVGA_FIFO_MIN is high enough to
    * leave room for them.
    *
    * These in block 3a, the VMX currently considers mandatory for the
    * extended FIFO.
    */

   // Valid if exists (i.e. if extended FIFO enabled):
   SVGA_FIFO_3D_HWVERSION,       /* See SVGA3dHardwareVersion in svga3d_reg.h */
   // Valid with SVGA_FIFO_CAP_PITCHLOCK:
   SVGA_FIFO_PITCHLOCK,

   // Valid with SVGA_FIFO_CAP_CURSOR_BYPASS_3:
   SVGA_FIFO_CURSOR_ON,          /* Cursor bypass 3 show/hide register */
   SVGA_FIFO_CURSOR_X,           /* Cursor bypass 3 x register */
   SVGA_FIFO_CURSOR_Y,           /* Cursor bypass 3 y register */
   SVGA_FIFO_CURSOR_COUNT,       /* Incremented when any of the other 3 change */
   SVGA_FIFO_CURSOR_LAST_UPDATED,/* Last time the host updated the cursor */

   // Valid with SVGA_FIFO_CAP_RESERVE:
   SVGA_FIFO_RESERVED,           /* Bytes past NEXT_CMD with real contents */

   /*
    * Valid with SVGA_FIFO_CAP_SCREEN_OBJECT or SVGA_FIFO_CAP_SCREEN_OBJECT_2:
    *
    * By default this is SVGA_ID_INVALID, to indicate that the cursor
    * coordinates are specified relative to the virtual root. If this
    * is set to a specific screen ID, cursor position is reinterpreted
    * as a signed offset relative to that screen's origin.
    */
   SVGA_FIFO_CURSOR_SCREEN_ID,

   /*
    * Valid with SVGA_FIFO_CAP_DEAD
    *
    * An arbitrary value written by the host, drivers should not use it.
    */
   SVGA_FIFO_DEAD,

   /*
    * Valid with SVGA_FIFO_CAP_3D_HWVERSION_REVISED:
    *
    * Contains 3D HWVERSION (see SVGA3dHardwareVersion in svga3d_reg.h)
    * on platforms that can enforce graphics resource limits.
    */
   SVGA_FIFO_3D_HWVERSION_REVISED,

   /*
    * XXX: The gap here, up until SVGA_FIFO_3D_CAPS, can be used for new
    * registers, but this must be done carefully and with judicious use of
    * capability bits, since comparisons based on SVGA_FIFO_MIN aren't
    * enough to tell you whether the register exists: we've shipped drivers
    * and products that used SVGA_FIFO_3D_CAPS but didn't know about some of
    * the earlier ones.  The actual order of introduction was:
    * - PITCHLOCK
    * - 3D_CAPS
    * - CURSOR_* (cursor bypass 3)
    * - RESERVED
    * So, code that wants to know whether it can use any of the
    * aforementioned registers, or anything else added after PITCHLOCK and
    * before 3D_CAPS, needs to reason about something other than
    * SVGA_FIFO_MIN.
    */

   /*
    * 3D caps block space; valid with 3D hardware version >=
    * SVGA3D_HWVERSION_WS6_B1.
    */
   SVGA_FIFO_3D_CAPS      = 32,
   SVGA_FIFO_3D_CAPS_LAST = 32 + 255,

   /*
    * End of VMX's current definition of "extended-FIFO registers".
    * Registers before here are always enabled/disabled as a block; either
    * the extended FIFO is enabled and includes all preceding registers, or
    * it's disabled entirely.
    *
    * Block 3b (truly optional extended registers): Additional registers for
    * the extended FIFO, which the VMX already knows how to enable and
    * disable with correct granularity.
    *
    * Registers after here exist if and only if the guest SVGA driver
    * sets SVGA_FIFO_MIN high enough to leave room for them.
    */

   // Valid if register exists:
   SVGA_FIFO_GUEST_3D_HWVERSION, /* Guest driver's 3D version */
   SVGA_FIFO_FENCE_GOAL,         /* Matching target for SVGA_IRQFLAG_FENCE_GOAL */
   SVGA_FIFO_BUSY,               /* See "FIFO Synchronization Registers" */

   /*
    * Always keep this last.  This defines the maximum number of
    * registers we know about.  At power-on, this value is placed in
    * the SVGA_REG_MEM_REGS register, and we expect the guest driver
    * to allocate this much space in FIFO memory for registers.
    */
    SVGA_FIFO_NUM_REGS
};


typedef enum {
   SVGA_CMD_INVALID_CMD = 0,
   SVGA_CMD_UPDATE = 1,
   SVGA_CMD_RECT_COPY = 3,
   SVGA_CMD_DEFINE_CURSOR = 19,
   SVGA_CMD_DEFINE_ALPHA_CURSOR = 22,
   SVGA_CMD_UPDATE_VERBOSE = 25,
   SVGA_CMD_FRONT_ROP_FILL = 29,
   SVGA_CMD_FENCE = 30,
   SVGA_CMD_ESCAPE = 33,
   SVGA_CMD_DEFINE_SCREEN = 34,
   SVGA_CMD_DESTROY_SCREEN = 35,
   SVGA_CMD_DEFINE_GMRFB = 36,
   SVGA_CMD_BLIT_GMRFB_TO_SCREEN = 37,
   SVGA_CMD_BLIT_SCREEN_TO_GMRFB = 38,
   SVGA_CMD_ANNOTATION_FILL = 39,
   SVGA_CMD_ANNOTATION_COPY = 40,
   SVGA_CMD_DEFINE_GMR2 = 41,
   SVGA_CMD_REMAP_GMR2 = 42,
   SVGA_CMD_MAX
} fifo_cmd_id;



pci_driver svga_driver = {
	.vendor = VMWARE_VENDOR_ID,
	.device = VMWARE_DEVICE_ID,
	.function = 0,
	.name = "VMware SVGA II Graphics Card",
	.init_device = svga_init_device,
	.irq_handler = NULL
};

static pci_function *func;

uint16_t vmw_svga_index;
uint16_t vmw_svga_value;						

// Prototype for test function
void screen_test();

void svga_out(uint16_t index, uint32_t value) {
	outl(index, vmw_svga_index);
	outl(value, vmw_svga_value);
}

int32_t svga_in(uint16_t index) {
	outl(index, vmw_svga_index);
	return inl(vmw_svga_value);
}

int svga_has_FIFO_cap(int32_t cap) {
	return (svga.fifo_buffer[SVGA_FIFO_CAPABILITIES] & cap) != 0;
}


/*
 * Initializes the VMware SVGA II
 *
 * INPUTS: the pci_function data structure corresponding to this device
 * OUTPUTS: 0 on success and -1 on failure
 */
int32_t svga_init_device(pci_function *func_) {
	// Store the pci_function pointer
	func = func_;
	
	// gets the svga index and value for port IO
	vmw_svga_index = func->reg_base[0] + SVGA_INDEX_PORT;
	vmw_svga_value = func->reg_base[0] + SVGA_VALUE_PORT;
	
	// Just to validate correct values and shit
    SVGA_DEBUG("   VMW SVGA INDEX: %x\n", vmw_svga_index);
	SVGA_DEBUG("   VMW SVGA VALUE: %x\n", vmw_svga_value);

    svga.device_id = SVGA_ID_2;
    do {
        svga_out(SVGA_REG_ID, svga.device_id);
        if (svga_in(SVGA_REG_ID) == svga.device_id) {
            break;
        } else {
            svga.device_id--;
        }
    } while (svga.device_id >= SVGA_ID_0);

    if (svga.device_id < SVGA_ID_0)
        SVGA_DEBUG("Error negotiating SVGA device version.");

    if (svga.device_id >= SVGA_ID_1)
        svga.capabilities = svga_in(SVGA_REG_CAPABILITIES);

    
    SVGA_DEBUG("   MAX WIDTH: %x    MAX HEIGHT: %x\n", svga_in(SVGA_REG_MAX_WIDTH), svga_in(SVGA_REG_MAX_HEIGHT));
	
    SVGA_DEBUG("   DEVICE VERSION ID: %x %x\n", svga.device_id, SVGA_ID_2);

	// Location of linear frame buffer in memory
	svga.frame_buffer = (void*) svga_in(SVGA_REG_FB_START);
	SVGA_DEBUG("   FRAME BUFFER: %x\n", svga.frame_buffer);

	// Location of fifo in memory
	svga.fifo_buffer = (void*) svga_in(SVGA_REG_MEM_START);
    SVGA_DEBUG("   FIFO: %x\n", svga.fifo_buffer);  

	// Page in the correct memory address regions
	if (svga_map_memory() == -1)
		return -1;

	// Setting resolution and bits per pixel
	svga_setmode(SYSTEM_RESOLUTION_WIDTH, SYSTEM_RESOLUTION_HEIGHT, SYSTEM_COLOR_DEPTH);

	// Size of linear frame buffer which happens to make no sense, but ok
	svga.frame_buffer_size = (uint32_t) svga_in(SVGA_REG_FB_SIZE);
	SVGA_DEBUG("   FRAME BUFFER SIZE: %d\n", svga.frame_buffer_size);

	// Size of fifo
	svga.fifo_size = svga_in(SVGA_REG_MEM_SIZE);
	SVGA_DEBUG("   FIFO SIZE: %d\n", svga.fifo_size);

    SVGA_DEBUG("   CAPABILITIES: %x\n", svga.capabilities);

    SVGA_DEBUG("   EXTENDED FIFO CAPABILITY: %x %x %x\n", svga.capabilities, SVGA_CAP_EXTENDED_FIFO, svga.capabilities & SVGA_CAP_EXTENDED_FIFO);

    svga.vram_size = svga_in(SVGA_REG_VRAM_SIZE);
    SVGA_DEBUG("   VRAM SIZE: %d\n", svga.vram_size);  

    svga.depth = svga_in(SVGA_REG_DEPTH);
    SVGA_DEBUG("   DEPTH: %d\n", svga.depth);  

    svga.pitch = svga_in(SVGA_REG_BYTES_PER_LINE);
    SVGA_DEBUG("   BYTES PER LINE: %d\n", svga.pitch); 



	// svga.fifo_buffer[SVGA_FIFO_MIN] = SVGA_FIFO_NUM_REGS * sizeof(uint32_t);
	// svga.fifo_buffer[SVGA_FIFO_MAX] = svga.fifo_size;
	// svga.fifo_buffer[SVGA_FIFO_NEXT_CMD] = svga.fifo_buffer[SVGA_FIFO_MIN];
	// svga.fifo_buffer[SVGA_FIFO_STOP] = svga.fifo_buffer[SVGA_FIFO_MIN];


    // Enable Hardware accelerated mouse
    svga_out(SVGA_REG_CURSOR_ID, 0);
    svga_out(SVGA_REG_CURSOR_Y, SYSTEM_RESOLUTION_HEIGHT / 2);
    svga_out(SVGA_REG_CURSOR_X, SYSTEM_RESOLUTION_WIDTH / 2);
    svga_out(SVGA_REG_CURSOR_ON, 1);

    // Set offscreen memory rectangle
    svga.offscreen.x1 = 0;
    svga.offscreen.y1 = SYSTEM_RESOLUTION_HEIGHT; //(svga.frame_buffer_size + svga.pitch - 1) / svga.pitch;
    svga.offscreen.x2 = svga.pitch / svga.depth;
    svga.offscreen.y2 = svga.vram_size / svga.pitch;

    return 0;    
}

void* svga_FIFO_reserve(uint32_t bytes) {
	volatile uint32_t *fifo = svga.fifo_buffer;
	uint32_t max = fifo[SVGA_FIFO_MAX];
	uint32_t min = fifo[SVGA_FIFO_MIN];
	uint32_t next_cmd = fifo[SVGA_FIFO_NEXT_CMD];

	int reserveable = svga_has_FIFO_cap(SVGA_FIFO_CAP_RESERVE);

    // printf("bytes: %d  max: %d  min: %d  next_cmd: %d  fifo size: %d\n", bytes, max, min, next_cmd, svga.fifo_size);

	if (bytes > (max - min))
		SVGA_PANIC("FIFO COMMAND TOO LARGE");
	if (bytes % sizeof(uint32_t))
		SVGA_PANIC("FIFO COMMAND NOT 32-BIT ALIGNED");
	if (svga.fifo.reserved_size != 0)
		SVGA_PANIC("FIFO RESERVE BEFORE FIFO COMMIT");

	svga.fifo.reserved_size = bytes;

	while (1) {
		uint32_t stop = fifo[SVGA_FIFO_STOP];
		int reserve_in_place = 0;
		int need_bounce = 0;
		
        // SVGA_DEBUG("next_cmd: %d    stop: %d    max: %d    min: %d    bytes: %d\n", next_cmd, stop, max, min, bytes);
		if (next_cmd >= stop) {

			if ((next_cmd + bytes < max) || 
				(next_cmd + bytes == max && stop > min)) {
				/* There is already enough contiguous space
				 * between nextCmd and max (the end of the buffer). */
				reserve_in_place = 1;
			}	
			else if ((max - next_cmd) + (stop - min) <= bytes) {
				// Not enough place for cmd
				svga_FIFO_full();
			}
			else 
				need_bounce = 1;
		}
		else {
			// FIFO data between next_cmd and max
			if (next_cmd + bytes < stop) {
				reserve_in_place = 1;
			}
			else 
				svga_FIFO_full();
		}
		
		if (reserve_in_place) {
			if (reserveable || bytes <= sizeof(uint32_t) * 5) {
				if (reserveable)
					fifo[SVGA_FIFO_RESERVED] = bytes;
				return next_cmd + (uint8_t*) fifo;
			}
			else
				need_bounce = 1;
		}
		if (need_bounce){
			// SVGA_PANIC("FUCKING BOUNCE BUFFER BULLSHIT\n")
            break;
        }
	}
    return NULL;
}

void svga_FIFO_commit(uint32_t bytes) {
    volatile uint32_t *fifo = svga.fifo_buffer;
    uint32_t next_cmd = fifo[SVGA_FIFO_NEXT_CMD];
    uint32_t max = fifo[SVGA_FIFO_MAX];
    uint32_t min = fifo[SVGA_FIFO_MIN];
    int reserveable = svga_has_FIFO_cap(SVGA_FIFO_CAP_RESERVE);

    if (svga.fifo.reserved_size == 0) 
        SVGA_PANIC("FIFO COMMIT FOR FIFO RESERVE");

    svga.fifo.reserved_size = 0;

    next_cmd += bytes;
    if (next_cmd >= max)
        next_cmd -= max - min;
    
    fifo[SVGA_FIFO_NEXT_CMD] = next_cmd;
    
    if (reserveable)
        fifo[SVGA_FIFO_RESERVED] = 0;
}

void svga_FIFO_commit_all() {
    svga_FIFO_commit(svga.fifo.reserved_size);
}

void* svga_FIFO_reserve_cmd(uint32_t type, uint32_t bytes) {
    uint32_t *cmd = svga_FIFO_reserve(bytes + sizeof(type));
    cmd[0] = type;
    return cmd + 1;
}

void svga_FIFO_full() {
    printf("FIFO FULL\n");

    svga_out(SVGA_REG_SYNC, 1);
    svga_in(SVGA_REG_BUSY);
}

/*
 * Send a 2D update rectangle through the FIFO
 *
 * INPUTS: coordinates and size
 * OUTPUTS: None
 */
void svga_update(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    return;
    svga_FIFO_cmd_update *cmd = svga_FIFO_reserve_cmd(SVGA_CMD_UPDATE, sizeof(*cmd));
    cmd->x = x;
    cmd->y = y;
    cmd->width = width;
    cmd->height = height;
    svga_FIFO_commit_all(); 
    svga_out(SVGA_REG_SYNC, 1);
    while(svga_in(SVGA_REG_BUSY)) {}   
}

void svga_rect_copy(uint32_t src_x, uint32_t src_y, uint32_t dest_x, uint32_t dest_y, uint32_t width, uint32_t height) {
    svga_FIFO_cmd_rect_copy *cmd = svga_FIFO_reserve_cmd(SVGA_CMD_RECT_COPY, sizeof(*cmd));
    cmd->src_x = src_x;
	cmd->src_y = src_y;
	cmd->dest_x = dest_x;
	cmd->dest_y = dest_y;
	cmd->width = width;
	cmd->height = height;
    svga_FIFO_commit_all(); 
    // svga_out(SVGA_REG_SYNC, 1);
    // while(svga_in(SVGA_REG_BUSY)){}   
}

int32_t svga_map_memory() {
	if (identity_map_containing_region((void*)svga.frame_buffer, svga.frame_buffer_size, PAGE_READ_WRITE | PAGE_GLOBAL | PAGE_DISABLE_CACHE) == -1)
		return -1;

	if (identity_map_containing_region((void*)svga.fifo_buffer, svga.fifo_size, PAGE_READ_WRITE | PAGE_GLOBAL | PAGE_DISABLE_CACHE) == -1)
		return -1;

	return 0;
}

void svga_enable() {
	// Commits the changes and changes resolution
	svga_out(SVGA_REG_ENABLE, 1);
	svga_out(SVGA_REG_CONFIG_DONE, 1);
}

void svga_disable() {
	svga_out(SVGA_REG_ENABLE, 0);
	// svga_out(SVGA_REG_CONFIG_DONE, 0);
}

void svga_setmode(uint32_t width, uint32_t height, uint32_t bpp) {
	svga.width = width;
	svga.height = height;
	svga.bpp = bpp;

	svga_out(SVGA_REG_WIDTH, width);
	svga_out(SVGA_REG_HEIGHT, height);
	svga_out(SVGA_REG_BITS_PER_PIXEL, bpp);
	svga.pitch = svga_in(SVGA_REG_BYTES_PER_LINE);

   	// Additional information if needed
	svga_in(SVGA_REG_FB_OFFSET);
	svga_in(SVGA_REG_DEPTH);
	svga_in(SVGA_REG_PSEUDOCOLOR);
	svga_in(SVGA_REG_RED_MASK);
	svga_in(SVGA_REG_GREEN_MASK);
	svga_in(SVGA_REG_BLUE_MASK);
}







