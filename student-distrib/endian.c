#include "endian.h"

/*
 * Converts a 16 bit value from big to little endian
 */
inline uint16_t b_to_l_endian16(uint16_t value) {
	return ((value & 0x00FF) << 8) | 
	       ((value & 0xFF00) >> 8);
}

/*
 * Converts a 32 bit value from big to little endian
 */
inline uint32_t b_to_l_endian32(uint32_t value) {
	return ((value & 0x000000FF) << 24) |
	       ((value & 0x0000FF00) << 8)  |
	       ((value & 0x00FF0000) >> 8)  |
	       ((value & 0xFF000000) >> 24);
}
