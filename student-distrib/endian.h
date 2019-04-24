#ifndef _ENDIAN_H
#define _ENDIAN_H

#include "types.h"

// Converts 16-bit or 32-bit values to the opposite endianness
inline uint16_t flip_endian16(uint16_t value);
inline uint32_t flip_endian32(uint32_t value);

#endif
