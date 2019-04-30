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
inline void spin_lock(struct spinlock_t *lock) {
	// Actually do nothing, since we are not running an SMP
}

/*
 * Unlocks the provided spinlock by setting it to zero (which is atomic)
 *
 * INPUTS: lock: the spinlock struct that this function will unlock
 * OUTPUTS: none
 * SIDE EFFECTS: unlocks the provided spinlock
 */
inline void spin_unlock(struct spinlock_t *lock) {
	// Do nothing, since we are not running an SMP
}
