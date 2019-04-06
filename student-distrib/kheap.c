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

// Memory descriptor
// Struct which will be stored along with allocated memory that keeps track of heap structure
typedef struct mem_desc {
	// Bit field to save memory
	struct {
		// The size of the block in bytes (23 bits because 2^22 = 4MB, the size of the heap, and the largest block is 4MB)
		//  including the descriptor
		unsigned int size: 23;
		// Whether or not this block is being used, boolean field
		unsigned int is_free: 9; 
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
	head->block_data.size = HEAP_SIZE & ((1 << 23) - 1);
	head->block_data.is_free = 1;
	head->next = NULL;
	head->prev = NULL;
	head->next_free = NULL;
	head->prev_free = NULL;

	spin_unlock(&heap_lock);
}

/*
 * Allocates a buffer of the specified size in the 4MiB kernel heap page and returns a pointer to it
 *
 * INPUTS: size: the size of the buffer to allocate
 * OUTPUTS: a pointer to a buffer of specified size
 */
void* kmalloc(uint32_t size) {
	spin_lock(&heap_lock);

	// Look for a free block in the linked list of free blocks
	mem_desc *cur, *prev;
	prev = NULL;
	for (cur = free_head; cur != NULL; cur = cur->next_free) {
		// Check if the block is exactly the right size
		if (cur->block_data.size == size + sizeof(mem_desc)) {
			// Then, simply mark the block as not free
			cur->block_data.is_free = 0;
			// Set prev->next and next->prev pointers to remove cur from the free list
			(cur->prev_free == NULL) ? (free_head = cur->next_free) : (cur->prev_free->next_free = cur->next_free);
			(cur->next_free == NULL) ? (free_tail = cur->prev_free) : (cur->next_free->prev_free = cur->prev_free);

			// Return the corresponding pointer
			spin_unlock(&heap_lock);
			return (void*)cur + sizeof(mem_desc);

		// We need space for one more block descriptor if the size desired is less
		//  than the size available, since we will have to break the block into 2
		} else if (cur->block_data.size >= size + 2 * sizeof(mem_desc)) {
			uint32_t new_size = size + sizeof(mem_desc);

			// Create the new block at the end
			mem_desc *new_block = (mem_desc*)((void*)cur + new_size);
			// Update next / prev pointers in the regular blocks array
			new_block->next = cur->next;
			new_block->prev = cur; 
			cur->next = new_block;
			// Mark the new block as free and set its size correctly
			new_block->block_data.is_free = 1;
			new_block->block_data.size = cur->block_data.size - new_size;

			// Swap the current block with the new block in the free blocks linked list
			new_block->next_free = cur->next_free;
			new_block->prev_free = cur->prev_free;
			// Update prev->next and next->prev to new_block instead of cur
			(cur->prev_free == NULL) ? (free_head = new_block) : (cur->prev_free->next_free = new_block);
			(cur->next_free == NULL) ? (free_tail = new_block) : (cur->next_free->prev_free = new_block);

			// Set the current block to have the correct values and return it
			cur->block_data.size = new_size;
			cur->block_data.is_free = 0;

			spin_unlock(&heap_lock);
			return (void*)cur + sizeof(mem_desc);
		}

		prev = cur;
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
		// Update next_free and next_free->prev_free if it exists
		first_free_block->next_free = free_block->next_free;
		if (free_block->next_free != NULL)
			free_block->next_free->prev_free = first_free_block;
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
