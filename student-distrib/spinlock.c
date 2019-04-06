#include "spinlock.h"
#include "lib.h"

// Spinlock implementation inspired by https://stackoverflow.com/questions/36731166/spinlock-with-xchg

/*
 * Attempts to lock the provided spinlock by trying to set it to 1 (using the atomic xchg operation)
 *  and proceeding if the value of the spinlock was 0, and repeating if it was already 1
 *
 * INPUTS: lock: the spinlock struct that this function will lock
 * OUTPUTS: none
 * SIDE EFFECTS: locks the provided spinlock
 */
inline void spin_lock(struct spinlock_t* lock) {
	asm volatile("\
		try_lock%=:       \n\
		movb $1, %%al     \n\
		xchgb %%al, %0    \n\
		testb %%al, %%al  \n\
		jnz try_lock%=\
		"
		: "=m"(lock->lock) // uses lock->lock as output
		: "m"(lock->lock) // uses lock->lock as input
		: "eax", "cc", "memory" // clobbers
	);
}

/*
 * Unlocks the provided spinlock by setting it to zero (which is atomic)
 *
 * INPUTS: lock: the spinlock struct that this function will unlock
 * OUTPUTS: none
 * SIDE EFFECTS: unlocks the provided spinlock
 */
inline void spin_unlock(struct spinlock_t* lock) {
	asm volatile("\
		movb $0, %%al   \n\
		xchgb %%al, %0  \n\
		"
		: "=m"(lock->lock)
		: "m"(lock->lock)
		: "eax", "memory"
	);
}

/*
 * Locks the provided spinlock while disabling interrupts and storing the current state of the flags
 *
 * INPUTS: lock: the spinlock struct that this function will lock
 * OUTPUTS: flags: the current state of the flags
 * SIDE EFFECTS: locks the provided spinlock and disables interrupts
 */
inline void spin_lock_irqsave(struct spinlock_t* lock, unsigned long *flags) {
	// Actually lock the lock
	spin_lock(lock);
	// Save the EFLAGS register into flags
	asm volatile("\
		pushfl         \n\
		popl %%eax     \n\
		movl %%eax, %0 \n\
	"
	: "=m"(*flags)
	:
	: "eax");
	// Mask interrupts
	cli();
}

/*
 * Unlocks the provided spinlock and restores the flags register back to what it was before locking
 *
 * INPUTS: lock: the spinlock struct that this function will unlock
 * OUTPUTS: none
 * SIDE EFFECTS: unlocks the provided spinlock and potentially re-enables interrupts
 */
inline void spin_unlock_irqsave(struct spinlock_t* lock, unsigned long flags) {
	// Unlock the lock
	spin_unlock(lock);
	// Restore the flags (potentially enabling interrupts but not necessarily)
	asm volatile("\
		pushl %0 \n\
		popfl    \n\
	"
	:
	: "m"(flags));
}
