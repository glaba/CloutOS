#include "paging.h"

static unsigned int kernel_page_directory[PAGE_DIRECTORY_SIZE];
static unsigned int video_page_table[PAGE_TABLE_SIZE] __attribute__((aligned (PAGE_ALIGNMENT)));

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
 * Unconditionally unmaps the 4MB aligned region containing the specified region 
 *  from the kernel page directory
 *
 * INPUTS: start_addr: the start address of the region to unmap
 *         size: the size of the region in bytes
 * SIDE EFFECTS: modifies the kernel page directory
 */
void unmap_region_from_kernel(void* start_addr, int size) {
	// The start address rounded down to the nearest 4MB (1 << 22 bytes)
	void* start_addr_aligned = (void*)((unsigned int)start_addr / (1 << 22) * (1 << 22));
	// The number of page directory entries to mark as present
	// We take the original # bytes and add on the extra bytes occupied by aligning to 4MB boundary
	//  and then divide by 4MB to get the number of page directory entries
	unsigned int num_pdes = (size + (unsigned int)(start_addr - start_addr_aligned)) / (1 << 22);

	// Mark all the desired pages as not present
	unsigned int i, cur_pde_index;
	for (i = 0; i < num_pdes; i++) {
		cur_pde_index = (unsigned int)(start_addr_aligned) / (1 << 22) + i;

		kernel_page_directory[cur_pde_index] = 0;
	}

	// Reload the page directory
	write_cr3(&kernel_page_directory);
}

/*
 * Finds a 4MB aligned region containing the specified region, adds it to the kernel page
 *  directory and flushes the TLB
 *
 * INPUTS: start_addr: the start address of the region to map in
 *         size: the size of the region in bytes
 *         flags: flags to assign to the page directory entry (PAGE_PRESENT and PAGE_SIZE_IS_4M added by default)
 * OUTPUTS: 0 if it succeeded or -1 if the region is being used for something else
 *           which includes the case where it's mapped into the kernel but not fully
 */
int map_region_into_kernel(void* start_addr, int size, unsigned int flags) {
	// The start address rounded down to the nearest 4MB (1 << 22 bytes)
	void* start_addr_aligned = (void*)((unsigned int)start_addr / (1 << 22) * (1 << 22));
	// The number of page directory entries to mark as present
	// We take the original # bytes and add on the extra bytes occupied by aligning to 4MB boundary
	//  and then divide by 4MB to get the number of page directory entries
	unsigned int num_pdes = (size + (unsigned int)(start_addr - start_addr_aligned)) / (1 << 22);

	// Check if the memory is partially mapped at any of the desired page directory entries, we fail if so
	unsigned int i, cur_pde_index, cur_pde;
	for (i = 0; i < num_pdes; i++) {
		cur_pde_index = (unsigned int)(start_addr_aligned) / (1 << 22);
		cur_pde = kernel_page_directory[cur_pde_index];

		// If the page is already present but it isn't a full 4MB page, then we don't
		//  want to mess with whatever is already going on there
		if ((cur_pde | PAGE_PRESENT) && !(cur_pde | PAGE_SIZE_IS_4M))
			return -1;
	}

	// Mark all the desired pages as 4M, present pages
	for (i = 0; i < num_pdes; i++) {
		cur_pde_index = (unsigned int)(start_addr_aligned) / (1 << 22) + i;

		kernel_page_directory[cur_pde_index] = (cur_pde_index * PAGE_TABLE_SIZE * PAGE_ALIGNMENT) |
			flags | PAGE_SIZE_IS_4M | PAGE_PRESENT;
	}

	// Reload the page directory
	write_cr3(&kernel_page_directory);

	return 0;
}

/*
 * Initializes the kernel page directory
 * Sets CR0 and CR4 to correctly support paging
 * Sets Page Directory Base Register (PDBR / CR3) to point to the kernel page directory
 *
 * INPUTS: none
 * OUTPUTS: none
 * SIDE EFFECTS: initializes paging for kernel 
 */
void init_paging() {
	int i;

	// Set the entries in video_page_table
	for (i = 0; i < PAGE_TABLE_SIZE; i++) {
		// If the current entry does not correspond to the region in video memory
		if (i < VIDEO / 4096 || i >= (VIDEO + VIDEO_SIZE) / 4096) {
			// The page is not present
			video_page_table[i] = ~PAGE_PRESENT | PAGE_READ_WRITE;
		} else {
			// The page is present
			video_page_table[i] = (i * 4096) | PAGE_READ_WRITE | PAGE_PRESENT;
		}
	}

	// Set page_directory[0] to point to the video page table
	kernel_page_directory[0] = (unsigned long)(&video_page_table) | PAGE_READ_WRITE | PAGE_PRESENT;

	// Map kernel memory starting at 4MB to a 4MB page
	kernel_page_directory[1] = KERNEL_START_ADDR | PAGE_GLOBAL | PAGE_SIZE_IS_4M | PAGE_CACHE | PAGE_READ_WRITE | PAGE_PRESENT;

	// Set all other page directory entries to not present
	for (i = 2; i < PAGE_DIRECTORY_SIZE; i++) {
		// Set bit 0 to zero, which means that the page table is not present
		kernel_page_directory[i] &= ~PAGE_PRESENT;
	}

	// Write the page directory to the page directory register
	write_cr3(&kernel_page_directory);

	// Enable 4MB pages
	enable_page_size_extension();

	// Enable paging
	enable_paging();
}
