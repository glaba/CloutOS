#ifndef _KHEAP_H
#define _KHEAP_H

#include "types.h"
#include "paging.h"

// Clears the entire heap and fills it with zeroes
void init_kheap();
// Allocates a buffer of the specified size in the kernel heap and returns a pointer to it
void* kmalloc(uint32_t size);
// Allocates a buffer of the specified size that is aligned to a multiple of the parameter alignment
void* kmalloc_aligned(uint32_t size, uint32_t alignment);
// Frees the memory associated with a pointer previously returned by kmalloc
// Double frees are not allowed in this implementation! 
void kfree(void* ptr);
// Lists all of the blocks allocated in the kernel heap (primarily for debugging)
void list_allocated_blocks();
// Lists all of the free blocks in the kernel heap (primarily for debugging)
void list_free_blocks();
// Verifies that none of the blocks in the linked list overlap (primarily for debugging)
void verify_no_overlaps();

#endif
