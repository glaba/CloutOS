#include "signals.h"
#include "processes.h"
#include "spinlock.h"
#include "pit.h"

#define GADGET_SIZE 7

// This is the x86 machine code for the following assembly which calls sigreturn:
//
//  movl $10, %eax
//  int 0x80
//
static uint32_t gadget[GADGET_SIZE] = {
	0xB8, 0x0A, 0x00, 0x00, 0x00, 0xCD, 0x80
};

static int signals_inited = 0;

/*
 * Called by the PIT every ten seconds so that programs can get the alarm signal
 */
void alarm_callback(double sys_time) {
	spin_lock_irqsave(pcb_spin_lock);

	int i;
	for (i = 0; i < pcbs.length; i++) {
		// Perform the NULL check even though it's not necessary to run this function more quickly
		if (pcbs.data[i].pid >= 0 && pcbs.data[i].signal_handlers[SIGNAL_ALARM] != NULL && pcbs.data[i].state != PROCESS_STOPPING) {
			send_signal(i, SIGNAL_ALARM, 0);
		}
	}

	spin_unlock_irqsave(pcb_spin_lock);
}

/*
 * Initializes signal functionality
 *
 * OUTPUTS: -1 on failure and 0 on success (failure need not stop the OS, but it will prevent alarm signal)
 */
int32_t init_signals() {
	// Register the alarm_callback with the PIT driver
	if (register_periodic_callback(PIT_FREQUENCY / SIGNAL_ALARM_FREQ, alarm_callback) == 0)
		return -1;

	signals_inited = 1;
	return 0;
}

/*
 * Sends a signal of given number to the process of given PID
 *
 * INPUTS: pid: the PID of the process which will get the signal
 *         signum: the number of the signal (from 0 to NUM_SIGNALS - 1)
 *         data: data to send along with the signal
 * OUTPUTS: -1 if the parameters were invalid and 0 if the signal was handled
 */
int32_t send_signal(int32_t pid, int32_t signum, uint32_t data) {
	// Check that the signal number is valid
	if (signum < 0 || signum >= NUM_SIGNALS)
		return -1;

	spin_lock_irqsave(pcb_spin_lock);

	// Check that the PID is valid
	if (pid < 0 || pid >= pcbs.length) {
		spin_unlock_irqsave(pcb_spin_lock);
		return -1;
	}

	// Check that the PID represents a running process
	if (pcbs.data[pid].pid < 0 || pcbs.data[pid].state == PROCESS_STOPPING) {
		spin_unlock_irqsave(pcb_spin_lock);
		return -1;
	}

	// Check if there is already a pending signal for the same signal number
	if (pcbs.data[pid].signal_status[signum] != SIGNAL_OPEN) {
		spin_unlock_irqsave(pcb_spin_lock);
		return -1;
	}

	// Mark the signal as pending and set its data
	pcbs.data[pid].signal_status[signum] = SIGNAL_PENDING;
	pcbs.data[pid].signal_data[signum] = data;

	spin_unlock_irqsave(pcb_spin_lock);
	return 0;
}

/*
 * Checks if there are any signals pending for the current process. If so, it performs the corresponding
 *  action, which may include changing the return address into the program to point to the signal handler
 */
void handle_signals() {
	spin_lock_irqsave(pcb_spin_lock);

	// Check to see if we can start signal handling
	if (!signals_inited || pcbs.length == 0) {
		spin_unlock_irqsave(pcb_spin_lock);
		return;
	}

	pcb_t *cur_pcb = get_pcb();

	// Find a pending signal
	int signum = -1;
	int i;
	for (i = 0; i < NUM_SIGNALS; i++) {
		if (cur_pcb->signal_status[i] == SIGNAL_PENDING) {
			signum = i;
			break;
		}
	}

	// Check if there are no pending signals
	if (signum == -1) {
		spin_unlock_irqsave(pcb_spin_lock);
		return;
	}

	// Make sure there are no other signals being handled
	for (i = 0; i < NUM_SIGNALS; i++) {
		if (cur_pcb->signal_status[i] == SIGNAL_HANDLING) {
			spin_unlock_irqsave(pcb_spin_lock);
			return;
		}
	}

	// Mark the signal as being handled
	cur_pcb->signal_status[signum] = SIGNAL_HANDLING;

	// Check if there is NOT a signal handler for this signal
	//  and perform the default action if so
	if (cur_pcb->signal_handlers[signum] == NULL) {
		switch (signum) {
			case SIGNAL_DIV_ZERO:
				process_halt(256);
				return;
			case SIGNAL_SEGFAULT:
				process_halt(256);
				return;
			case SIGNAL_INTERRUPT:
				process_halt(0);
				return;
			case SIGNAL_ALARM:
			case SIGNAL_IO:
				break;
		}

		spin_unlock_irqsave(pcb_spin_lock);
		return;
	}

	// In this case, there is a signal handler
	// Get the address of the userspace stack
	uint8_t *new_esp = (uint8_t*)get_user_context()->esp;

	// Push onto the userspace stack so that the stack looks like this
	//
	//   return address, which points to the gadget
	//   signum (parameter to handler function)
	//   data (parameter to handler function)
	//   entire h/w context of userspace process (a process_context struct)
	//   gadget which calls sigreturn to clean up this stack
	//   <previous stack contents>
	//

	// Check that everything we will push will fit in the userspace program
	if (is_userspace_region_valid(new_esp - 3 * sizeof(uint32_t) - sizeof(process_context) - GADGET_SIZE,
	                                        3 * sizeof(uint32_t) + sizeof(process_context) + GADGET_SIZE,
	                               get_pid()) != 0) {
		spin_unlock_irqsave(pcb_spin_lock);
		return;
	}


	// First, push the gadget
	new_esp -= GADGET_SIZE;
	for (i = 0; i < GADGET_SIZE; i++)
		new_esp[i] = gadget[i];

	// Record the return address for the signal handler, which should point to the start of the gadget
	uint32_t ret_addr = (uint32_t)new_esp;

	// Then, push the hardware context
	new_esp -= sizeof(process_context);
	*(process_context*)new_esp = *get_user_context();

	// Lastly, push the signal number and the return address to the gadget
	new_esp -= sizeof(uint32_t); *(uint32_t*)new_esp = cur_pcb->signal_data[signum];
	new_esp -= sizeof(uint32_t); *(uint32_t*)new_esp = signum;
	new_esp -= sizeof(uint32_t); *(uint32_t*)new_esp = ret_addr;

	// Set the new value of esp, eip for when we return back to userspace from this current invocation
	//  of the kernel
	process_context *c = get_user_context();
	c->esp = (uint32_t)new_esp;
	c->eip = (uint32_t)cur_pcb->signal_handlers[signum];

	spin_unlock_irqsave(pcb_spin_lock);
	return;
}

/*
 * Cleans up any modifications made to the userspace stack due to a signal handler being called
 *  and restores the process state to how it was before the signal handler was called for the process
 *  corresponding to the current kernel stack
 */
void cleanup_signal() {
	spin_lock_irqsave(pcb_spin_lock);

	// Get the current userspace stack pointer, which should be pointing to a stack that looks like this
	// 
	//   signum
	//   data
	//   entire h/w context (a process_context struct)
	//   gadget which called sigreturn
	//   <previous stack contents>
	//
	void *esp = (uint8_t*)get_user_context()->esp;

	// Get the signal number from the userspace stack
	int32_t signum = *(int32_t*)esp;
	// Mark the signal as open
	get_pcb()->signal_status[signum] = SIGNAL_OPEN;

	// Get the process_context from the userspace stack
	process_context *new_context = (process_context*)(esp + 2 * sizeof(uint32_t));
	// Set that as the new process_context, which should restore the entire userspace program back to its
	//  old state
	*get_user_context() = *new_context;

	spin_unlock_irqsave(pcb_spin_lock);
}
