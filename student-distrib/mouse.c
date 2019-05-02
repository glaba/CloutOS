#include "lib.h"
#include "i8259.h"
#include "keyboard.h"
#include "kheap.h"
#include "irq_defs.h"
#include "graphics/VMwareSVGA.h"
#include "graphics/graphics.h"
#include "mouse.h"
#include "window_manager/window_manager.h"

#define MOUSE_PORT   			0x60
#define MOUSE_STATUS 			0x64
#define MOUSE_ABIT   			0x02
#define MOUSE_BBIT   			0x01
#define MOUSE_WRITE  			0xD4
#define SAMPLE_RATE_CMD 		0xF3
#define VALID_MOUSE_INTERRUPT   0x3D

#define RIGHT_MOUSE_CLICK		0x2	
#define LEFT_MOUSE_CLICK		0x1

mouse_info *mouse;

#define abs(n) ((n < 0) ? (-n) : (n))


void mouse_wait(uint8_t a_type) {
	uint32_t timeout = 100000;
	if (!a_type) {
		while (--timeout) {
			if ((inb(MOUSE_STATUS) & MOUSE_BBIT) == 1) {
				return;
			}
		}
		printf("mouse timeout\n");
		return;
	} else {
		while (--timeout) {
			if (!((inb(MOUSE_STATUS) & MOUSE_ABIT))) {
				return;
			}
		}
		printf("mouse timeout\n");
		return;
	}
}

void mouse_write(uint8_t write) {
	mouse_wait(1);
	outb(MOUSE_WRITE, MOUSE_STATUS);
	mouse_wait(1);
	outb(write, MOUSE_PORT);
}

uint8_t mouse_read() {
	mouse_wait(0);
	char t = inb(MOUSE_PORT);
	return t;
}

// Initializes the mouse to use interrupts and enables the mouse interrupt
void init_mouse() {
    printf("Initing mouse\n");
	uint8_t status;
	mouse_wait(1);
	outb(0xA8, MOUSE_STATUS);
	mouse_wait(1);
	outb(0x20, MOUSE_STATUS);
	mouse_wait(0);
	status = inb(0x60) | 2;
	mouse_wait(1);
	outb(0x60, MOUSE_STATUS);
	mouse_wait(1);
	outb(status, MOUSE_PORT);
	mouse_write(0xF6);
	mouse_read();
	mouse_write(0xF4);
	mouse_read();

	// Magic sequence to change mouse ID to 3
	set_mouse_rate(200); 
	set_mouse_rate(100);
	set_mouse_rate(80);

	// Reading Mouse ID
	mouse_write(0xF2);
	mouse_read(0);
    printf("Mouse ID: %x\n", inb(MOUSE_PORT));

	// Just init both to middle of screen
	mouse->x = SYSTEM_RESOLUTION_WIDTH / 2;
	mouse->y = SYSTEM_RESOLUTION_HEIGHT / 2;
	mouse->scroll = 0;
	mouse->holding_window = 0;

    enable_irq(MOUSE_IRQ);
}

void set_mouse_rate(int8_t rate) {
	// cli();
	outb(MOUSE_WRITE, MOUSE_STATUS);                    
	outb(SAMPLE_RATE_CMD, MOUSE_PORT);                    
	while(!(inb(MOUSE_STATUS) & 1)) {}
	mouse_read(0);                    
	outb(MOUSE_WRITE, MOUSE_STATUS);                    
	outb(rate, MOUSE_PORT);                     
	while(!(inb(MOUSE_STATUS) & 1)) {} 
	mouse_read(0);    
	// sti();                 
}

int mouse_cycle = -5;              
int32_t x = SYSTEM_RESOLUTION_WIDTH / 2;
int32_t y = SYSTEM_RESOLUTION_HEIGHT / 2;
// Interrupt handler for IRQ12 (mouse interrupt)
void mouse_handler() {
	send_eoi(MOUSE_IRQ);
	int8_t state;    
	int8_t mouse_x;         
	int8_t mouse_y;
	int8_t scroll_wheel;
	if (mouse_cycle < 0 || inb(MOUSE_STATUS) == VALID_MOUSE_INTERRUPT) {
		switch(mouse_cycle) {
			case 0:
				state = (int8_t) inb(MOUSE_PORT);
				// printf("STATE: %d\n", state);
				mouse_cycle++;
				return;
			case 1:
				mouse_x = (int8_t) inb(MOUSE_PORT);
				// printf("X: %d\n", mouse_x);
				mouse_cycle++;
				return;
			case 2:
				mouse_y = (int8_t) inb(MOUSE_PORT);
				// printf("Y: %d\n", mouse_y);
				mouse_cycle++;
				return;
			case 3:
				scroll_wheel = (int8_t) inb(MOUSE_PORT);
				// printf("SCROLL: %d\n", scroll_wheel);
				mouse_cycle = 0;
				break;
			default:
				mouse_cycle = 0;
				return;
		}
	}

	x = x + mouse_x;
    y = y - mouse_y;
	
	mouse->x = x;
	mouse->y = y;


	mouse->scroll += scroll_wheel;
	mouse->right_click = 0;
	mouse->left_click = 0;
	bound_mouse_coordinates();
	if (state & RIGHT_MOUSE_CLICK) {
		mouse->right_click = 1;
		// mouse_event(mouse->x, mouse->y);
	}
	if (state & LEFT_MOUSE_CLICK) {
		mouse->left_click = 1;
		// mouse_event(mouse->x, mouse->y);
	}

	mouse_event(mouse->x, mouse->y);

	// printf("Mouse Data: x: %d   y: %d   scroll: %d   left: %d   right: %d\n", mouse->x, mouse->y, mouse->scroll, mouse->left_click, mouse->right_click);
	draw_pixel(svga.frame_buffer, svga.width, mouse->x, mouse->y, 0xFFFFFFFF);
	svga_update(0, 0, svga.width, svga.height);
	mouse->old_x = mouse->x;
	mouse->old_y = mouse->y;
}

void bound_mouse_coordinates() {
	if (mouse->x <= 0)
		mouse->x = 0;
	if (mouse->y <= 0)
		mouse->y = 0;
	if (mouse->x >= SYSTEM_RESOLUTION_WIDTH)
		mouse->x = SYSTEM_RESOLUTION_WIDTH;
	if (mouse->y >= SYSTEM_RESOLUTION_HEIGHT)
		mouse->y = SYSTEM_RESOLUTION_HEIGHT;		
}


