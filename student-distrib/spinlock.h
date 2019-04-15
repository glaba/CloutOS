#ifndef _SPINLOCK_H
#define _SPINLOCK_H

#include "types.h"

// Struct representing a spinlock, which really only holds a bit of information
struct spinlock_t {
	volatile uint8_t lock;
};

// Constant that represents an unlocked spinlock
#define SPIN_LOCK_UNLOCKED {0};

// Locks the provided spinlock (does not consider interrupts)
inline void spin_lock(struct spinlock_t* lock);
// Unlocks the provided spinlock (does not consider interrupts)
inline void spin_unlock(struct spinlock_t* lock);

#endif 
