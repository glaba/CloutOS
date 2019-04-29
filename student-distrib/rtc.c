#include "rtc.h"
#include "lib.h"
#include "i8259.h"
#include "processes.h"
#include "kheap.h"

// Each process can have a virtual RTC device associated with it, which is described
//  by this struct
typedef struct rtc_client {
	// The PID of the processes
	int32_t pid;
	// The number of RTC ticks that must pass for this process to finish its read
	// Set by rtc_write
	int interval;
	// Whether or not the client is waiting to be woken up
	int waiting;
} rtc_client;

// We will keep the metadata in a linked list
typedef LIST_ITEM(rtc_client, rtc_client_list_item) rtc_client_list_item;

// A list of processes that have an RTC device open
rtc_client_list_item *rtc_client_list_head = NULL;

// Indicates whether or not initialization has been called
static int init = 0;

// The counter that keeps track of the number of ticks at 1024 Hz
static volatile int counter = 0;

/*
 * NMI_enable()
 * Enables non-maskable interrupts
 *
 * INPUTS: none
 * OUTPUTS: none
 * SIDE EFFECTS: Enables the non-maskable interrupt
 */
void NMI_enable() {
	outb(inb(RTC_ADDRESS_PORT) & NMI_ENABLE_MASK, RTC_ADDRESS_PORT);
}
/*
 * NMI_disable()
 * Disable non-maskable interrupts
 *
 * INPUTS: none
 * OUTPUTS: none
 * SIDE EFFECTS: Disables the non-maskable interrupt
 */
void NMI_disable() {
	outb(inb(RTC_ADDRESS_PORT) | NMI_DISABLE_MASK, RTC_ADDRESS_PORT);
}

/*
 * init_rtc()
 * Initialies the RTC registers and enables the interrupt
 *
 * INPUTS: none
 * OUTPUTS: none
 * SIDE EFFECTS: Initializes the RTC
 */
void init_rtc() {
	/* Disables NMI to prevent undefined behavior */
	NMI_disable();

	/* Enables interrupt generation in RTC */
	outb(REGISTER_A, RTC_ADDRESS_PORT);
	outb(RTC_INTERRUPT_ENABLE_CMD, RTC_DATA_PORT);

	/* Turns on IR8 periodic interrupt */
	outb(REGISTER_B, RTC_ADDRESS_PORT);
	char prev_data = inb(RTC_DATA_PORT);
	outb(REGISTER_B, RTC_ADDRESS_PORT);
	outb(prev_data | 0x40, RTC_DATA_PORT);

	// Set to the maximum frequency so that we can divide the frequency for any processes reading RTC
	set_freq(BASE_FREQ);

	/* Enables NMI again */
	NMI_enable();

	/* Init keeps track of whether init_rtc() was called or not */
	init = 1;
	
	enable_irq(RTC_IRQ);
}

/*
 * set_freq()
 * Sets the frequency to what input says
 *
 * INPUTS: int32_t f: frequency RTC is
 *                    supposed to be set
 *                    to
 * OUTPUTS: -1 for FAIL, otherwise 0
 * SIDE EFFECTS: sets freq
 */
int32_t set_freq(int32_t f) {
	/* Use freq to store real freq byte to write */
	int32_t freq;
	/* Use switch to determine which freqency to set freq to */
	switch (f) {
		case 2:    freq = _2HZ_;    break;
		case 4:    freq = _4HZ_;    break;
		case 8:    freq = _8HZ_;    break;
		case 16:   freq = _16HZ_;   break;
		case 32:   freq = _32HZ_;   break;
		case 64:   freq = _64HZ_;   break;
		case 128:  freq = _128HZ_;  break;
		case 256:  freq = _256HZ_;  break;
		case 512:  freq = _512HZ_;  break;
		case 1024: freq = _1024HZ_; break;
		default: return -1;
	}

	/* Changing the interrupt rate */
	char prev_data;
	outb(REGISTER_A, RTC_ADDRESS_PORT);
	prev_data = inb(RTC_DATA_PORT);
	outb(REGISTER_A, RTC_ADDRESS_PORT);
	outb((prev_data & 0xF0) | freq, RTC_DATA_PORT);

	/* Return 4 bytes written */
	return 4;
}

/*
 * rtc_handler()
 * Handler which gets called every time RTC sends an interrupt
 *
 * INPUTS: none
 * OUTPUTS: none
 * SIDE EFFECTS: Handler for the RTC
 */
void rtc_handler() {
	/* Select register C */
	outb(REGISTER_C, RTC_ADDRESS_PORT);
	/* Throw away contents they don't matter :( */
	inb(RTC_DATA_PORT);
	/* Send EOI */
	send_eoi(RTC_IRQ);

	// Update the counter
	counter = (counter + 1) % BASE_FREQ;

	// Go through the list of all RTC clients and wake up those that should be triggered
	rtc_client_list_item *cur;
	for (cur = rtc_client_list_head; cur != NULL; cur = cur->next) {
		// Check if the correct interval has passed and if the process was waiting
		if (cur->data.waiting && counter % cur->data.interval == 0) {
			// If so, wake up the process
			cur->data.waiting = 0;
			process_wake(cur->data.pid);
		}
	}
}

/*
 * rtc_open()
 * Does necessary stuff to initialize/start rtc
 *
 * INPUTS: filename: unused
 * OUTPUTS: 0 for pass and -1 for failure
 * SIDE EFFECTS: Initializes RTC if not initialized
 *               and sets freq to 2 HZ
 */
int32_t rtc_open(const uint8_t *filename) {
	/* Clear interrupts so nothing stops us from setting freq */
	cli();

	/* Call init_rtc if not yet initialized */
	if (!init)
		init_rtc();

	// Look through the linked list to make sure that the RTC isn't already opened by this process
	int32_t pid = get_pid();
	rtc_client_list_item *cur;
	for (cur = rtc_client_list_head; cur != NULL; cur = cur->next) {
		// If the current PID is equal to a PID in the list, we fail
		if (cur->data.pid == pid) {
			sti();
			return -1;
		}
	}

	// Allocate memory for the linked list node
	rtc_client_list_item *client = kmalloc(sizeof(rtc_client_list_item));
	if (client == NULL) {
		sti();
		return -1;
	}

	// Fill in the fields of the linked list node
	client->data.pid = pid;
	client->data.interval = BASE_FREQ / DEFAULT_FREQ;
	client->data.waiting = 0;

	// Push the linked list node into the linked list
	client->next = rtc_client_list_head;
	rtc_client_list_head = client;
	
	// Re-enable interrupts and return success
	sti();
	return 0;
}

/*
 * rtc_close()
 * Does necessary stuff to close rtc
 *
 * INPUTS: fd: unused
 * OUTPUTS: 0 for pass
 * SIDE EFFECTS: N/A
 */
int32_t rtc_close(int32_t fd) {
	int32_t pid = get_pid();

	// Block interrupts while we modify the linked list
	cli();

	// Find the item in the linked list corresponding to the current process
	rtc_client_list_item *prev, *cur;
	for (prev = NULL, cur = rtc_client_list_head; 
	     cur != NULL; 
	     prev = cur, cur = cur->next) {

		// Look for the node with the same PID
		if (cur->data.pid == pid) {
			// Remove the item from the linked list
			if (prev == NULL)
				rtc_client_list_head = cur->next;
			else
				prev->next = cur->next;

			// Free the linked list node
			kfree(cur);
			break;
		}
	}

	// Re-enable interrupts now that we are done touching the linked list
	sti();
	return 0;
}

/*
 * rtc_read()
 * Waits for an RTC interrupt
 *
 * INPUTS: int32_t fd = file descriptor
 *         void* buf = buffer that's useless for now
 *         int32_t bytes = number of bytes in buf
 * OUTPUTS: 0 for pass
 * SIDE EFFECTS: Waits for RTC interrupt
 */
int32_t rtc_read(int32_t fd, void *buf, int32_t bytes) {
	// Block interrupts while we use the linked list
	cli();

	// Find the item in the RTC linked list corresponding to this PID
	int32_t pid = get_pid();
	rtc_client_list_item *cur, *item;
	for (cur = rtc_client_list_head; cur != NULL; cur = cur->next) {
		// If the PID matches, store the item
		if (cur->data.pid == pid) {
			item = cur;
			break;
		}
	}

	// Mark the item as waiting
	item->data.waiting = 1;
	sti();

	// Put the process to sleep and let the RTC handler take care of waking it up
	process_sleep(pid);

	// When the process is woken up, it will return here and back to the userspace program
	return 0;
}

/*
 * rtc_write()
 * Sets frequency to what's specified in buf
 *
 * INPUTS: int32_t fd = file descriptor
 *         const void* buf = buffer that contains frequency
 *         int32_t bytes = number of bytes in buf (should be 4)
 * OUTPUTS: the number of bytes written, or -1 if the input was bad
 * SIDE EFFECTS: Waits for RTC interrupt
 */
int32_t rtc_write(int32_t fd, const void *buf, int32_t bytes) {
	if (buf == NULL || bytes != 4)
		return -1;

	// Read the frequency as a 32-bit integer from the buffer
	int32_t freq;
	freq = *(int32_t*)buf;

	// Validate that the frequency is a power of 2 between 2 and 1024
	//  using a nice binary trick
	if (freq < 2 || freq > 1024 || (freq & (freq - 1)) != 0)
		return -1;

	// Clear interrupts before we modify items in the linked list
	cli();

	// Find the item in the linked list corresponding to the current PID and set the interval
	//  field to correspond to the frequency
	int32_t pid = get_pid();
	rtc_client_list_item *cur;
	for (cur = rtc_client_list_head; cur != NULL; cur = cur->next) {
		if (cur->data.pid == pid) {
			// Set the interval to be 1024 / freq, which gives the number of ticks at 1024 Hz
			//  to skip to get a frequency of freq
			cur->data.interval = BASE_FREQ / freq;

			// In some sense, we have "written" 4 bytes (the frequency) to the virtual RTC device
			sti();
			return sizeof(int32_t);
		}
	}

	// If we got here, the RTC was not opened before being written to, so return -1
	sti();
	return -1;
}
