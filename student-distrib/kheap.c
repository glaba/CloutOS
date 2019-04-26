#include "kheap.h"
#include "spinlock.h"
#include "lib.h"

/***************************************************************************************/
/*                                                                                     */
/*          This entire malloc algorithm was inspired by the incredible and            */
/*          deep work of the only CS in Eng resident of 1109 W Stoughton St            */
/*                                  whose name is                                      */
/*                                                                                     */
/*            __________               __         .__                                  */
/*            \______   \ ____   _____/  |______  |  |__ _____    ____                 */
/*             |       _// __ \_/ __ \   __\__  \ |  |  \\__  \  /    \                */
/*             |    |   \  ___/\  ___/|  |  / __ \|   Y  \/ __ \|   |  \               */
/*             |____|_  /\___  >\___  >__| (____  /___|  (____  /___|  /               */
/*                    \/     \/     \/          \/     \/     \/     \/                */
/*                                                                                     */
/*    _____         __   .__                            .__.__                         */
/*   /     \  __ __|  | _|  |__   ____ ___________    __| _|  |__ ___._______  ___.__. */
/*  /  \ /  \|  |  |  |/ |  |  \ /  _ \\____ \__  \  / __ ||  |  <   |  \__  \<   |  | */
/* /    Y    |  |  |    <|   Y  (  <_> |  |_> / __ \/ /_/ ||   Y  \___  |/ __ \\___  | */
/* \____|__  |____/|__|_ |___|  /\____/|   __(____  \____ ||___|  / ____(____  / ____| */
/*         \/           \/    \/       |__|       \/     \/     \/\/         \/\/      */
/*                                                                                     */
/***************************************************************************************/

// The threshold between what we will call a "small" block and a "big" block
// Small blocks are placed in the beginning of the free linked list when they are freed
//  and big blocks are placed at the end, which encourages small blocks to be allocated
//  before fragmenting bigger ones
#define BIG_BLOCK_THRESHOLD 2000

// Memory descriptor
// Struct which will be stored along with allocated memory that keeps track of heap structure
typedef struct mem_desc {
	// Bit field to save memory
	struct {
		// The size of the block in bytes (31 bits since we are not covering over half the entire RAM with a heap)
		//  *including the descriptor*
		unsigned int size: 31;
		// Whether or not this block is being used, boolean field
		unsigned int is_free: 1; 
	} block_data;
	// Pointer to the next memory descriptor 
	struct mem_desc *next, *prev;
	// If this block is free, pointer to the next free node
	struct mem_desc *next_free, *prev_free;
} mem_desc;

static mem_desc *head = NULL;
static mem_desc *free_head = NULL;
static mem_desc *free_tail = NULL;

static struct spinlock_t heap_lock = SPIN_LOCK_UNLOCKED;

/*
 * Clears the entire heap, fills it with zeroes, and initializes values
 */
void init_kheap() {
	spin_lock(&heap_lock);

	// Fill the heap with zeroes
	int i;
	uint32_t *heap_base = (uint32_t*)KERNEL_HEAP_START_ADDR;
	for (i = 0; i < HEAP_SIZE / 4; i++) {
		heap_base[i] = 0;
	}

	// Set all the heads and tails to point to the singular block starting at the beginning
	head = (mem_desc*)KERNEL_HEAP_START_ADDR;
	free_head = (mem_desc*)KERNEL_HEAP_START_ADDR;
	free_tail = (mem_desc*)KERNEL_HEAP_START_ADDR;

	// Set the single block to be free and contain the whole heap
	head->block_data.size = HEAP_SIZE & 0x7FFFFFFF;
	head->block_data.is_free = 1;
	head->next = NULL;
	head->prev = NULL;
	head->next_free = NULL;
	head->prev_free = NULL;

	spin_unlock(&heap_lock);
}

/*
 * Removes the provided mem_desc from the free linked list
 */
void remove_free_element(mem_desc *cur) {
	// Set prev->next and next->prev pointers to remove cur from the free list
	if (free_head == cur)
		free_head = cur->next_free;
	else
		cur->prev_free->next_free = cur->next_free;

	if (free_tail == cur)
		free_tail = cur->prev_free;
	else
		cur->next_free->prev_free = cur->prev_free;
}

/*
 * Splits the provided free block into two blocks, where the first block has provided size 
 *
 * INPUTS: cur: the block to split
 *         size: the size of the first block in the resulting split (excluding descriptor)
 * OUTPUTS: NULL if the block is not free or if the size is invalid, address of second block in split otherwise
 */
mem_desc* split_free_block(mem_desc *cur, uint32_t size) {
	// Check that the size is valid
	if (size + 2 * sizeof(mem_desc) > cur->block_data.size)
		return NULL;

	// Check that the block is free
	if (!cur->block_data.is_free)
		return NULL;

	uint32_t total_size = cur->block_data.size;

	// Update the size of the first block
	cur->block_data.size = size + sizeof(mem_desc); 

	// Get a pointer to the address of the new block
	mem_desc *new_block = (mem_desc*)((void*)cur + size + sizeof(mem_desc));
	// Mark it free
	new_block->block_data.is_free = 1;
	// Set the size of the second block to be 
	//  total original size - size of first block's data - size of first block's descriptor
	new_block->block_data.size = total_size - size - sizeof(mem_desc);

	// Update all pointers in the regular blocks array, inserting new_block in correct memory order
	new_block->next = cur->next;
	new_block->prev = cur;
	cur->next = new_block;
	(new_block->next == NULL) ? (0) : (new_block->next->prev = new_block);

	// Update all pointers in the free blocks array, inserting new_block at the tail of the free array
	// Insert the new block at the head if it is small and at the tail if it is big
	// The intent of this is to prevent fragmentation by trying to use small blocks before big ones
	if (new_block->block_data.size > BIG_BLOCK_THRESHOLD) {
		// Put the block at the end of the linked list
		new_block->next_free = NULL;
		new_block->prev_free = free_tail;
		(free_tail == NULL) ? (free_head = new_block) : (free_tail->next_free = new_block);
		free_tail = new_block;
	} else {
		// Put the block at the beginning of the linked list
		new_block->prev_free = NULL;
		new_block->next_free = free_head;
		(free_head == NULL) ? (free_tail = new_block) : (free_head->prev_free = new_block);
		free_head = new_block;
	}

	return new_block;
}

/*
 * Allocates a buffer of the specified size that is aligned to a multiple of the parameter alignment
 *
 * INPUTS: size: the size of the buffer to allocate
 *         alignment: the returned pointer will be a multiple of this value
 * OUTPUTS: a pointer to an aligned buffer of the specified size
 */
void* kmalloc_aligned(uint32_t size, uint32_t alignment) {
	spin_lock(&heap_lock);

	// Look through all the free blocks
	mem_desc *cur;
	for (cur = free_head; cur != NULL; cur = cur->next_free) {
		// [start, end): bounds on the actual region of allocatable memory given by this block
		uint32_t start = (uint32_t)cur + sizeof(mem_desc);
		uint32_t end = (uint32_t)cur + cur->block_data.size;

		// The block contains an aligned address if
		//   start = alignment * m OR
		//   start = alignment * m + j and end = alignment * n + k for n > m, k < alignment, j < alignment
		// The two following if statements check for these two cases, respectively
		if (start % alignment == 0) {
			// In this case, we either have exactly the right size, 
			//  or we can split into two blocks and reserve the first
			if (cur->block_data.size == size + sizeof(mem_desc) ||
				split_free_block(cur, size) != NULL) {
				
				// Then, simply mark the block as not free and remove it from the free list
				cur->block_data.is_free = 0;
				remove_free_element(cur);

				// Return the corresponding pointer
				spin_unlock(&heap_lock);
				return (void*)cur + sizeof(mem_desc);
			}
		} else if (start / alignment != end / alignment) {
			// In this case, the buffer should start from alignment * (start / alignment + 1)
			//  which is basically start rounded up to the closest multiple of alignment
			uint32_t actual_start = alignment * (start / alignment + 1);

			// Currently, we have
			//  [ mem_desc | (...) ]
			//             ^
			//             start
			// where ... indicates some unknown number of bytes and () means optional. We want 
			//  [ mem_desc | (...) | mem_desc | size bytes | (mem_desc) | (...) ]
			//             ^                  ^
			//             start              actual_start

			// Let's check if we have enough room to fit all these things
			// First check: the space between start and actual_start can fit a mem_desc
			if (actual_start - start < sizeof(mem_desc))
				continue;
			// Second check: either the first and second block fully fill the entire area
			//               or the third block does exist, and there is enough room for its mem_desc
			int enough_room = (sizeof(mem_desc) + (actual_start - start) + size == cur->block_data.size) ||
			                  (2 * sizeof(mem_desc) + (actual_start - start) + size <= cur->block_data.size);
			if (!enough_room)
				continue;

			// Note down whether or not we will have to create a third block (ie, the first and second DON'T
			//  fully fill the original block)
			int third_block = !(sizeof(mem_desc) + (actual_start - start) + size == cur->block_data.size);

			// Perform the first split, such that the first piece has data size indicated in the diagram
			mem_desc *second_block = split_free_block(cur, actual_start - start - sizeof(mem_desc));
		
			// Then, perform an additional split if a third block needs to be created
			if (third_block)
				split_free_block(second_block, size);
		
			// Mark the second block (the aligned one with the desired size) as not free and remove it
			//  from the free blocks list
			second_block->block_data.is_free = 0;
			remove_free_element(second_block);

			// Return the corresponding pointer
			spin_unlock(&heap_lock);
			return (void*)second_block + sizeof(mem_desc);
		}
	}

	spin_unlock(&heap_lock);
	return NULL;
}

/*
 * Allocates a buffer of the specified size in the kernel heap area and returns a pointer to it
 *
 * INPUTS: size: the size of the buffer to allocate
 * OUTPUTS: a pointer to a buffer of specified size
 */
void* kmalloc(uint32_t size) {
	spin_lock(&heap_lock);

	// Look for a free block in the linked list of free blocks
	mem_desc *cur;
	for (cur = free_head; cur != NULL; cur = cur->next_free) {
		// Check if the block is exactly the right size
		//  or if we can try splitting the current block into two pieces, 
		//  where the first piece is of the desired size
		if (cur->block_data.size == size + sizeof(mem_desc) ||
			split_free_block(cur, size) != NULL) {

			// Then, simply mark the block as not free and remove it from the free list
			cur->block_data.is_free = 0;
			remove_free_element(cur);

			// Return the corresponding pointer
			spin_unlock(&heap_lock);
			return (void*)cur + sizeof(mem_desc);
		}
	}

	// If no free block was found, since we have a fixed size heap, return NULL
	spin_unlock(&heap_lock);
	return NULL;
}

/*
 * Frees the memory associated with a pointer previously returned by kmalloc
 * Double frees are not allowed in this implementation! 
 *
 * INPUTS: ptr: a pointer previously returned by kmalloc
 */
void kfree(void* ptr) {
	// Typical behavior for free is to ignore NULL ptr
	if (ptr == NULL)
		return;

	spin_lock(&heap_lock);

	// Get memory descriptor corresponding to this pointer and mark it free
	mem_desc *cur = (mem_desc*)((void*)ptr - sizeof(mem_desc));
	cur->block_data.is_free = 1;

	// Push the block to the end of the free linked list
	if (free_tail == NULL) {
		free_head = cur;
		free_tail = cur;
		cur->next_free = NULL;
		cur->prev_free = NULL;
	} else {
		cur->next_free = NULL;
		cur->prev_free = free_tail;
		free_tail->next_free = cur;
		free_tail = cur;
	}

	// Attempt to coalesce all contiguous free blocks
	// Find the first free block going backwards
	mem_desc *free_block, *first_free_block;
	for (free_block = cur; free_block != NULL && free_block->block_data.is_free; free_block = free_block->prev) {
		first_free_block = free_block;
	}

	// Starting from after the first free block, coalesce free blocks
	for (free_block = first_free_block->next; free_block != NULL; free_block = free_block->next) {
		// If the block isn't free, we can't coalesce any more
		if (!free_block->block_data.is_free)
			break;

		// Merge this block with first_free_block in the main blocks list
		// Update next and next->prev if it exists
		first_free_block->next = free_block->next;
		if (free_block->next != NULL)
			free_block->next->prev = first_free_block;

		// Update the size of the block to have the combined size of both
		first_free_block->block_data.size += free_block->block_data.size;

		// Merge this block with first_free_block in the free blocks list by simply removing it
		(free_block->next_free == NULL) ? (free_tail = free_block->prev_free) : 
		                                  (free_block->next_free->prev_free = free_block->prev_free);
		(free_block->prev_free == NULL) ? (free_head = free_block->next_free) : 
		                                  (free_block->prev_free->next_free = free_block->next_free);
	}

	spin_unlock(&heap_lock);
}

/*
 * Lists all of the blocks allocated in the kernel heap
 * Primarily intended for debugging memory leaks
 */
void list_allocated_blocks() {
	printf("LISTING ALL ALLOCATED BLOCKS\n");
	mem_desc *cur;
	for (cur = head; cur != NULL; cur = cur->next) {
		if (!cur->block_data.is_free) {
			printf("   ADDR: 0x%x   SIZE: 0x%x\n", cur, cur->block_data.size);
		}
	}
}

/*
 * Lists all of the free blocks in the kernel heap
 * Primarily intended for debugging memory leaks
 */
void list_free_blocks() {
	printf("LISTING ALL FREE BLOCKS\n");
	mem_desc *cur;
	for (cur = free_head; cur != NULL; cur = cur->next_free) {
		printf("   ADDR: 0x%x   SIZE: 0x%x\n", cur, cur->block_data.size);
	}
}

/*
 * Verifies that none of the blocks in the linked list overlap
 */
void verify_no_overlaps() {
	void *cur_addr = (void*)KERNEL_HEAP_START_ADDR;
	mem_desc *cur;
	for (cur = head; cur != NULL; cur = cur->next) {
		if (cur_addr != (void*)cur) {
			printf("OVERLAP FOUND!\n");
			return;
		}

		cur_addr += cur->block_data.size;
	}
}
