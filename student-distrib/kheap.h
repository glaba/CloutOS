#ifndef _KHEAP_H
#define _KHEAP_H

#include "types.h"
#include "paging.h"

// Clears the entire heap and fills it with zeroes
void init_kheap();
// Allocates a buffer of the specified size in the 4MiB kernel heap page and returns a pointer to it
void* kmalloc(uint32_t size);
// Frees the memory associated with a pointer previously returned by kmalloc
// Double frees are not allowed in this implementation! 
void kfree(void* ptr);
// Lists all of the blocks allocated in the kernel heap (primarily for debugging)
void list_allocated_blocks();
// Lists all of the free blocks in the kernel heap (primarily for debugging)
void list_free_blocks();

#endif
