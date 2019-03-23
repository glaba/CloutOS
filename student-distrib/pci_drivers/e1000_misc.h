#ifndef _E1000_MISC_H
#define _E1000_MISC_H

// Uncomment ETH_DEBUG_ENABLE to enable debugging
#define ETH_DEBUG_ENABLE
#ifdef ETH_DEBUG_ENABLE
	#define ETH_DEBUG(f, ...) printf(f, ##__VA_ARGS__)
#else
	#define ETH_DEBUG(f, ...) // Nothing
#endif

// Accesses a 32-bit value in a uint8_t array starting at the given offset
#define GET_32(arr, offset) *((uint32_t*)((arr) + (offset)))
// Accesses a 16-bit value in a uint8_t array starting at the given offset
#define GET_16(arr, offset) *((uint16_t*)((arr) + (offset)))
// Accesses a 8-bit value in a uint8_t array starting at the given offset
#define GET_8(arr, offset) (arr)[offset]

#endif
