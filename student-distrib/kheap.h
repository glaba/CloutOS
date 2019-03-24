#ifndef _KHEAP_H
#define _KHEAP_H

#include "types.h"

// The kernel heap starts at 8MB in physical memory
#define KERNEL_HEAP_START_ADDR 0x800000
// The size of the heap is one large page (4 MiB)
#define HEAP_SIZE 0x400000

// Clears the entire heap and fills it with zeroes
void kclear_heap();
// Allocates a buffer of the specified size in the 4MiB kernel heap page and returns a pointer to it
void* kmalloc(uint32_t size);
// Frees the memory associated with a pointer previously returned by kmalloc
// Double frees are not allowed in this implementation! 
void kfree(void* ptr);

#endif
