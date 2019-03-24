#include "kheap.h"

// Memory descriptor
// Struct which will be stored along with allocated memory that keeps track of heap structure
typedef struct mem_desc {
	// Bit field to save memory
	struct {
		// The size of the block in bytes (22 bits because 2^22 = 4MB, the size of the heap)
		//  including the descriptor
		unsigned int size: 22;
		// Whether or not this block is being used, boolean field
		unsigned int is_free: 10; 
	} block_data;
	// Pointer to the next memory descriptor 
	struct mem_desc* next;
} mem_desc;

static mem_desc *head = NULL;

/*
 * Clears the entire heap and fills it with zeroes
 */
void kclear_heap() {
	int i;
	uint32_t *heap_base = (uint32_t*)KERNEL_HEAP_START_ADDR;
	for (i = 0; i < HEAP_SIZE / 4; i++) {
		heap_base[i] = 0;
	}
	head = NULL;
}

/*
 * Allocates a buffer of the specified size in the 4MiB kernel heap page and returns a pointer to it
 *
 * INPUTS: size: the size of the buffer to allocate
 * OUTPUTS: a pointer to a buffer of specified size
 */
void* kmalloc(uint32_t size) {
	mem_desc *cur, *prev;

	prev = NULL;
	for (cur = head; cur != NULL; cur = cur->next) {
		prev = cur;

		if (!cur->block_data.is_free)
			continue;

		// Attempt to coalesce cur with all contiguous free blocks
		mem_desc *free_block;

		// Loop through the blocks following cur
		for (free_block = cur->next; free_block != NULL; free_block = free_block->next) {
			// If the block isn't free, we can't coalesce any more
			if (!free_block->block_data.is_free)
				break;

			// Merge this block with cur
			cur->next = free_block->next;
			cur->block_data.size += free_block->block_data.size;
		}

		// Check if cur is now large enough for the requested block
		if (cur->block_data.size == size + sizeof(mem_desc)) {
			// It's exactly large enough, so just use cur as is
			cur->block_data.is_free = 0;
			return (void*)cur + sizeof(mem_desc);
		// We need space for 2 metadatas, since we will be splitting this block into 2,
		//  creating a new free block at the end that did not exist before
		} else if (cur->block_data.size >= size + 2 * sizeof(mem_desc)) {
			uint32_t new_size = size + sizeof(mem_desc);

			// Create the new block at the end
			mem_desc *new_block = (mem_desc*)((void*)cur + new_size);
			// Update next pointers
			new_block->next = cur->next; 
			cur->next = new_block;
			// Mark the new block as free and set its size correctly
			new_block->block_data.is_free = 1;
			new_block->block_data.size = cur->block_data.size - new_size;
			
			// Set the current block to have the correct values and return it
			cur->block_data.size = new_size;
			cur->block_data.is_free = 0;
			return (void*)cur + sizeof(mem_desc);
		}
	}

	// If we have reached here, it is because there was no free block to use
	//  so we have to try to put it at the end
	if (prev == NULL) {
		// This means the linked list was empty, so start at the beginning of the heap
		// First, check if it will fit within the heap
		if (size + sizeof(mem_desc) > HEAP_SIZE)
			return NULL;

		head = (mem_desc*)KERNEL_HEAP_START_ADDR;
		head->block_data.size = size + sizeof(mem_desc);
		head->block_data.is_free = 0;
		head->next = NULL;
		return (void*)head + sizeof(mem_desc);
	} else {
		// We just have to create a new block after prev
		// First, check if it will fit
		if ((void*)prev + prev->block_data.size + sizeof(mem_desc) + size > (void*)(KERNEL_HEAP_START_ADDR + HEAP_SIZE))
			return NULL;

		// Create a new block at the end
		mem_desc *new_block = (mem_desc*)((void*)prev + prev->block_data.size);
		// Update next pointers
		new_block->next = NULL;
		prev->next = new_block;
		// Mark the new block as not free, set its size, and return it
		new_block->block_data.is_free = 0;
		new_block->block_data.size = size + sizeof(mem_desc);
		return (void*)new_block + sizeof(mem_desc);
	}
}

/*
 * Frees the memory associated with a pointer previously returned by kmalloc
 * Double frees are not allowed in this implementation! 
 *
 * INPUTS: ptr: a pointer previously returned by kmalloc
 */
void kfree(void* ptr) {
	// Retrieve the mem_desc corresponding to this pointer
	mem_desc *desc = (mem_desc*)(ptr - sizeof(mem_desc));

	// Mark it as free
	desc->block_data.is_free = 1;
}
