#include "paging.h"

static unsigned int page_directory[PAGE_DIRECTORY_SIZE];
static unsigned int video_page_table[PAGE_TABLE_SIZE] __attribute__((aligned (PAGE_ALIGNMENT)));
static unsigned int user_video_page_table[PAGE_TABLE_SIZE] __attribute__((aligned (PAGE_ALIGNMENT)));

// A struct which represents a large 4MB page in physical memory, and keeps track of other pages near it
typedef struct large_page {
	// Whether or not the page is in use
	uint8_t used;
	// If this page is not in use, the index of another page that is not in use,
	//  which should form a linked list of unused pages. The last item in the list points to -1
	// If this page IS in use, this value is a don't care
	int32_t next_free;
} large_page;

// An array of large_page structures, in order of position in physical memory
// So, the first item corresponds to the page from 0MB-4MB, the second corresponds to 4MB-8MB, etc
large_page large_pages[LAST_ACCESSIBLE_ADDR / LARGE_PAGE_SIZE];

// The index of the head of the linked list of unused pages
int32_t unused_page_head_index;

/*
 * Writes a page directory address to the cr3 register
 *
 * INPUT: pd_addr: the address of the page directory, which is
 *                 a 1024 item array of 4-byte entries
 */
#define write_cr3(pd_addr)            \
do {                                  \
	asm volatile ("movl %k0, %%cr3"   \
		:                             \
		: "r"((pd_addr))              \
		: "cc"                        \
	);                                \
} while (0)

/*
 * Enables paging by setting bit 31 in CR0
 */
static inline void enable_paging() {
	asm volatile ("           \n\
		mov %%cr0, %%eax      \n\
		or $0x80000000, %%eax \n\
		mov %%eax, %%cr0"
		:
		:
		: "eax", "cc"
	);
}

/*
 * Enables 4MB pages by setting bit 4 (PSE) of CR4
 */
static inline void enable_page_size_extension() {
	asm volatile ("       \n\
		mov %%cr4, %%eax  \n\
		or $0x10, %%eax   \n\
		mov %%eax, %%cr4"
		:
		:
		: "eax", "cc"
	);
}

/*
 * If there is no mapping already existing, maps a region of specified size starting from the
 *  given virtual address to the region of the same size starting from the given physical address
 *
 * INPUTS: start_phys_addr: the start of the region in physical memory
 *         start_virt_addr: the start of the region in virtual memory
 *         num_pdes: the size of the region in multiples of 4MB
 *         flags: the flags that should be applied to the PDE (4MB page and present will be included by default)
 * OUTPUTS: 0 on success and -1 if the region was already being used
 */
int32_t map_region(void *start_phys_addr, void *start_virt_addr, uint32_t num_pdes, uint32_t flags) {
	// Align the two inputs to the nearest 4MB boundary (if they are not already)
	start_phys_addr = (void*)(((uint32_t)start_phys_addr / LARGE_PAGE_SIZE) * LARGE_PAGE_SIZE);
	start_virt_addr = (void*)(((uint32_t)start_virt_addr / LARGE_PAGE_SIZE) * LARGE_PAGE_SIZE);

	// Check if the memory is mapped at any of the desired page directory entries, we fail if so
	unsigned int i, cur_pde_index, cur_pde;

	// Mark all the desired pages as 4M, present pages, as well as the custom flags
	unsigned int cur_phys_index;
	for (i = 0; i < num_pdes; i++) {
		cur_pde_index = (unsigned int)(start_virt_addr) / LARGE_PAGE_SIZE + i;
		cur_phys_index = (unsigned int)(start_phys_addr) / LARGE_PAGE_SIZE + i;

		page_directory[cur_pde_index] = (cur_phys_index * PAGE_TABLE_SIZE * PAGE_ALIGNMENT) |
			flags | PAGE_SIZE_IS_4M | PAGE_PRESENT;
	}

	// Reload the page directory
	write_cr3(&page_directory);

	return 0;
}

/*
 * Unconditionally unmaps the 4MB aligned region containing the specified region
 *  from the page directory
 *
 * INPUTS: start_addr: the start virtual address of the region to unmap
 *         num_pdes: the size of the region in 4MB increments
 * SIDE EFFECTS: modifies the page directory
 */
void unmap_region(void *start_addr, uint32_t num_pdes) {
	// The start address rounded down to the nearest 4MB (1 << 22 bytes)
	void *start_addr_aligned = (void*)((unsigned int)start_addr / LARGE_PAGE_SIZE * LARGE_PAGE_SIZE);

	// Mark all the desired pages as not present
	unsigned int i, cur_pde_index;
	for (i = 0; i < num_pdes; i++) {
		cur_pde_index = (unsigned int)(start_addr_aligned) / LARGE_PAGE_SIZE + i;

		page_directory[cur_pde_index] = 0;
	}

	// Reload the page directory
	write_cr3(&page_directory);
}

/*
 * If there is no mapping already existing, maps in a 4MB-aligned region made up of large 4MB pages that fully contains
 *  the desired region. Both start_phys_addr and start_virt_addr are assumed to have the same offset mod 4MB
 *
 * INPUTS: start_phys_addr: the start of the region in physical memory
 *         start_virt_addr: the start of the region in virtual memory
 *         size: the size of the region in bytes
 *         flags: the flags that should be applied to the PDE (4MB page and present will be included by default)
 * OUTPUTS: 0 on success and -1 if the region was already being used or if the pointers are not correctly aligned
 */
int32_t map_containing_region(void *start_phys_addr, void *start_virt_addr, uint32_t size, uint32_t flags) {
	// Check if physical address and virtual address are offset by the same amount from a multiple of 4MB
	if ((uint32_t)start_phys_addr % LARGE_PAGE_SIZE != (uint32_t)start_virt_addr % LARGE_PAGE_SIZE)
		return -1;

	// The start addresses rounded down to the nearest 4MB (1 << 22 bytes)
	void *start_phys_addr_aligned = (void*)((unsigned int)start_phys_addr / LARGE_PAGE_SIZE * LARGE_PAGE_SIZE);
	void *start_virt_addr_aligned = (void*)((unsigned int)start_virt_addr / LARGE_PAGE_SIZE * LARGE_PAGE_SIZE);
	// The number of page directory entries to mark as present
	// We take the original # bytes and add on the extra bytes occupied by aligning to 4MB boundary
	//  and then divide by 4MB to get the number of page directory entries
	uint32_t num_pdes = (size + (uint32_t)start_phys_addr % LARGE_PAGE_SIZE) / LARGE_PAGE_SIZE + 1;

	// Map the given region
	return map_region(start_phys_addr_aligned, start_virt_addr_aligned, num_pdes, flags);
}

/*
 * Unconditionally unmaps the smallest 4MB-aligned region made up of large 4MB pages containing the given region
 *
 * INPUTS: start_addr: the starting address of the given region
 *         size: the size of the given region in bytes
 * SIDE EFFECTS: modifies the page directory
 */
void unmap_containing_region(void *start_addr, uint32_t size) {
	uint32_t num_pdes = (size + (uint32_t)start_addr % LARGE_PAGE_SIZE) / LARGE_PAGE_SIZE + 1;

	unmap_region(start_addr, num_pdes);
}

/*
 * Finds a 4MB aligned region containing the specified region, identity maps it to the page
 *  directory and flushes the TLB
 *
 * INPUTS: start_addr: the start address of the region to map in
 *         size: the size of the region in bytes
 *         flags: flags to assign to the page directory entry (PAGE_PRESENT and PAGE_SIZE_IS_4M added by default)
 * OUTPUTS: 0 if it succeeded or -1 if the region is being used for something else
 *           which includes the case where it's mapped into the kernel but not fully
 */
int32_t identity_map_containing_region(void *start_addr, uint32_t size, unsigned int flags) {
	return map_containing_region(start_addr, start_addr, size, flags);
}

/*
 * Maps an open virtual region into video memory so that a userspace program can access it
 *
 * OUTPUTS: addr: the virtual address that video memory is mapped to
 * RETURNS: 0 on success and -1 otherwise
 */
int32_t map_video_mem_user(void **addr) {
	// We will just put this in the 4MB region from 192MB - 196MB for now
	page_directory[VIDEO_USER_VIRT_ADDR >> 22] = (unsigned long)(&user_video_page_table) |
		PAGE_READ_WRITE | PAGE_USER_LEVEL | PAGE_PRESENT;

	// Return the address that it was mapped to
	*addr = (void*)VIDEO_USER_VIRT_ADDR;

	// Reload the page directory
	write_cr3(&page_directory);

	// We always succeed, for now
	return 0;
}

/*
 * Unmaps the video memory paged in for userspace programs 
 *
 * INPUTS: addr: the virtual address that the video memory was paged into
 */
void unmap_video_mem_user(void *addr) {
	page_directory[(uint32_t)addr >> 22] = 0;

	// Reload the page directory
	write_cr3(&page_directory);
}

/*
 * Returns the index of an unused 4MB page in physical memory and marks it used
 *
 * OUTPUTS: -1 if no page could be found, and the index of the page otherwise
 */
int32_t get_open_page() {
	// Simply pick the head of the unused page linked list
	int32_t page = unused_page_head_index;
	if (page < 0)
		return -1;

	// Mark the page as used
	large_pages[page].used = 1;

	// Update the head of the linked list to be the next free page
	unused_page_head_index = large_pages[page].next_free;

	// Return the index we found
	return page;
}

/*
 * Marks the page at the provided index as unused
 *
 * INPUTS: index: the index of the large page to mark as unused, in increments of 4MB (ex. 2 corresponds to 8MB)
 */
void free_page(int32_t index) {
	// Validate that the index is valid
	if (index < 0 || index >= LAST_ACCESSIBLE_ADDR / LARGE_PAGE_SIZE)
		return;

	large_pages[index].used = 0;

	// Set this to point to the old head, and let this be the new head of the unused page linked list
	large_pages[index].next_free = unused_page_head_index;
	unused_page_head_index = index;
}

/*
 * Initializes the page directory
 * Sets CR0 and CR4 to correctly support paging
 * Sets Page Directory Base Register (PDBR / CR3) to point to the page directory
 *
 * INPUTS: none
 * OUTPUTS: none
 * SIDE EFFECTS: initializes paging for kernel
 */
void init_paging() {
	int i;

	// Set the entries in video_page_table as well as user_video_page_table
	for (i = 0; i < PAGE_TABLE_SIZE; i++) {
		// If the current entry does not correspond to the region in video memory
		if (i < VIDEO / NORMAL_PAGE_SIZE || i >= (VIDEO + VIDEO_SIZE) / NORMAL_PAGE_SIZE) {
			// The page is not present
			video_page_table[i] = ~PAGE_PRESENT;
			user_video_page_table[i] = ~PAGE_PRESENT;
		} else {
			// The page is present
			video_page_table[i] = (i * NORMAL_PAGE_SIZE) | PAGE_READ_WRITE | PAGE_PRESENT;
			user_video_page_table[i] = (i * NORMAL_PAGE_SIZE) | PAGE_USER_LEVEL | PAGE_READ_WRITE | PAGE_PRESENT;
		}
	}

	// Set page_directory[0] to point to the video page table and mark large page #0 as used
	page_directory[0] = (unsigned long)(&video_page_table) | PAGE_DISABLE_CACHE | PAGE_READ_WRITE | PAGE_PRESENT;
	large_pages[0].used = 1;

	// Map kernel memory starting at 4MB to a 4MB page and mark large page #1 as used
	page_directory[1] = KERNEL_START_ADDR | PAGE_GLOBAL | PAGE_SIZE_IS_4M | PAGE_READ_WRITE | PAGE_PRESENT;
	large_pages[1].used = 1;

	// Map in kernel heap memory
	identity_map_containing_region((void*)KERNEL_HEAP_START_ADDR, HEAP_SIZE,
		KERNEL_HEAP_START_ADDR | PAGE_GLOBAL | PAGE_SIZE_IS_4M | PAGE_READ_WRITE | PAGE_PRESENT);
	// Mark the kernel heap memory as used
	for (i = KERNEL_HEAP_START_ADDR / LARGE_PAGE_SIZE; i < KERNEL_HEAP_END_ADDR / LARGE_PAGE_SIZE; i++)
		large_pages[i].used = 1;

	// Set all other page directory entries to not present
	for (i = KERNEL_HEAP_END_ADDR / LARGE_PAGE_SIZE; i < PAGE_DIRECTORY_SIZE; i++) {
		// Set bit 0 to zero, which means that the page is not present
		page_directory[i] &= ~PAGE_PRESENT;

		// Mark the page as unused as well, if it exists in physical memory
		if (i < LAST_ACCESSIBLE_ADDR / LARGE_PAGE_SIZE) {
			large_pages[i].used = 0;

			// Connect all the unused pages with linked list connections
			if (i == LAST_ACCESSIBLE_ADDR / LARGE_PAGE_SIZE - 1)
				large_pages[i].next_free = -1;
			else
				large_pages[i].next_free = i + 1;
		}
	}

	// Mark the head of the linked list of unused page to the end of the heap
	unused_page_head_index = KERNEL_HEAP_END_ADDR / LARGE_PAGE_SIZE;

	// Write the page directory to the page directory register
	write_cr3(&page_directory);

	// Enable 4MB pages
	enable_page_size_extension();

	// Enable paging
	enable_paging();
}
