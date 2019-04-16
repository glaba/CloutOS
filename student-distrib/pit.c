#include "pit.h"
#include "lib.h"
#include "i8259.h"
#include "irq_defs.h"
#include "list.h"
#include "kheap.h"
#include "spinlock.h"

static struct spinlock_t pit_spin_lock = SPIN_LOCK_UNLOCKED;

// Structure that stores a callback and associated information
typedef struct callback_t {
	void (*callback)(double);
	uint32_t id;
	int interval;
	int counter;
} callback_t;

typedef LIST_ITEM(callback_t) callback_list_item;

callback_list_item *callback_list_head;

// Precomputed interval to increment by so we don't have to do floating point division
//  on every timer interrupt
static double interval;

// Time elapsed from system startup
double sys_time = 0.0;

/*
 * Initializes the Programmable Interval Timer to generate interrupts at a frequency of very close to 69 Hz
 */
void init_pit() {
	spin_lock(&pit_spin_lock);

	// Initialize the callback linked list
	callback_list_head = NULL;

	spin_unlock(&pit_spin_lock);

	// Enable interrupts for the PIT
	enable_irq(TIMER_IRQ);

	// Write the desired configuration to the PIT command port
	outb(PIT_CMD_CHANNEL_0 | PIT_CMD_ACCESS_MODE_LO_HI | PIT_CMD_MODE_2 | PIT_CMD_BINARY_MODE, PIT_CMD_REGISTER);

	// Write the countdown timer with the desired value
	// First write the bottom 8 bits
	outb(PIT_RELOAD_VALUE & 0xFF, PIT_CHANNEL_0_DATA_PORT);
	// Then the upper 8 bits
	outb((PIT_RELOAD_VALUE >> 8) & 0xFF, PIT_CHANNEL_0_DATA_PORT);

	// Set the interval to be 1/frequency
	interval = 1.0 / PIT_FREQUENCY;
}

/*
 * Adds a callback to the list of callbacks that will be called at a periodic interval, returning
 *  an ID that will be used to deregister the callback
 * Callbacks must run quickly!
 *
 * INPUTS: interval: the number of timer ticks that pass between invocations of this callback, where
 *                   each timer tick takes 1/PIT_FREQUENCY seconds
 *         callback: a function to call at the given interval, which will be given the current value of time
 *                   as a parameter
 * OUTPUTS: an id corresponding to the callback, or 0 on failure
 */
uint32_t register_periodic_callback(int interval, void (*callback_fn)(double)) {
	if (callback_fn == NULL)
		return 0;

	callback_list_item *callback = kmalloc(sizeof(callback_list_item));

	callback->data.callback = callback_fn;
	callback->data.interval = interval;
	callback->data.counter = interval;

	// Acquire the lock for modifying the linked list
	spin_lock(&pit_spin_lock);

	// Find an id for it that is not already present in the linked list
	// The linked list will be sorted by ID, so we can exploit this to find an ID in linear time
	uint32_t id;
	callback_list_item *cur, *prev;
	for (prev = NULL, cur = callback_list_head, id = 1; 
		 cur != NULL; 
		 prev = cur, cur = cur->next, id++) {

		if (cur->data.id != id)
			break;
	}

	// Set the ID and insert the callback into the linked list
	callback->data.id = id;
	if (prev == NULL) {
		callback->next = callback_list_head;
		callback_list_head = callback;
	} else {
		callback->next = cur;
		prev->next = callback;
	}

	spin_unlock(&pit_spin_lock);

	return callback->data.id;
}

/*
 * Unregisters a previously registered callback
 *
 * INPUTS: id: the ID returned by register_periodic_callback when the callback was registered
 */
void unregister_periodic_callback(uint32_t id) {
	// Acquire the lock for modifying the linked list
	spin_lock(&pit_spin_lock);

	// Search through the list for the callback with the specified ID
	callback_list_item *cur, *prev;
	for (prev = NULL, cur = callback_list_head; 
		 cur != NULL; 
		 prev = cur, cur = cur->next) {

		// If this element has the correct ID, remove it from the linked list
		if (cur->data.id == id) {
			// Check if it is the head or not
			if (prev == NULL)
				callback_list_head = cur->next;
			else
				prev->next = cur->next;	
			
			// Free the memory allocated for the linked list item
			kfree(cur);
			break;
		}
	}

	spin_unlock(&pit_spin_lock);
}

/*
 * Handler for the timer interrupt
 */
void timer_handler() {
	send_eoi(TIMER_IRQ);

	sys_time += interval;

	// Go through all the handlers
	callback_list_item *cur;
	for (cur = callback_list_head; cur != NULL; cur = cur->next) {
		// Decrement the counter
		cur->data.counter--;

		// Once the counter has reached zero, reset the counter and fire the callback
		if (cur->data.counter == 0) {
			cur->data.counter = cur->data.interval;
			cur->data.callback(sys_time);
		}
	}
}
