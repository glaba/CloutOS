#ifndef _SPINLOCK_H
#define _SPINLOCK_H

#include "types.h"

/*
 * Locks the provided spinlock and disables interrupts, saving the EFLAGS register in flags
 *
 * INPUTS: lock: a spinlock_t that represents atomic access to some resource
 *         flags: a variable that will be filled in with the flags register
 */
#define spin_lock_irqsave(lock) \
do { \
	cli_and_save(lock.flags); \
	spin_lock(&lock); \
} while (0)

/*
 * Unlocks the provided spinlock and restores the state of the EFLAGS register to what it was
 *  before the spinlock was locked
 *
 * INPUTS: lock: a spinlock_t that represents atomic access to some resource that was locked
 *         flags: the filled in value of the flags register from spin_lock_irqsave
 */
#define spin_unlock_irqsave(lock) \
do { \
	spin_unlock(&lock); \
	restore_flags(lock.flags); \
} while (0)

// Struct representing a spinlock, which really only holds a bit of information
struct spinlock_t {
	uint32_t flags;
};

// Constant that represents an unlocked spinlock
#define SPIN_LOCK_UNLOCKED {0};

// Locks the provided spinlock (does not consider interrupts)
inline void spin_lock(struct spinlock_t* lock);
// Unlocks the provided spinlock (does not consider interrupts)
inline void spin_unlock(struct spinlock_t* lock);

#endif 
