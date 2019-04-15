#ifndef _ENDIAN_H
#define _ENDIAN_H

#include "types.h"

// Converts 16-bit or 32-bit values from big to little endian
inline uint16_t b_to_l_endian16(uint16_t value);
inline uint32_t b_to_l_endian32(uint32_t value);

#endif