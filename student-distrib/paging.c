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
 * If there is no mapping already existing, maps a region of specified size starting from the
 *  given virtual address to the region of the same size starting from the given physical address
 * INPUTS: start_phys_addr: the start of the region in physical memory
 *         start_virt_addr: the start of the region in virtual memory
 *         size: the size of the region in multiples of 4MB 
 *         flags: the flags that should be applied to the PDE (4MB page and present will be included by default)
 * OUTPUTS: 0 on success and -1 if the region was already being used
 */
int32_t map_region(void *start_phys_addr, void *start_virt_addr, uint32_t size, uint32_t flags) {
	// Align the two inputs to the nearest 4MB boundary (if they are not already)
	start_phys_addr = (void*)(((uint32_t)start_phys_addr / 0x400000) * 0x400000);
	start_virt_addr = (void*)(((uint32_t)start_virt_addr / 0x400000) * 0x400000);

	// Check if the memory is mapped at any of the desired page directory entries, we fail if so
	unsigned int i, cur_pde_index, cur_pde;
	for (i = 0; i < size; i++) {
		cur_pde_index = (unsigned int)(start_virt_addr) / (1 << 22) + i;
		cur_pde = kernel_page_directory[cur_pde_index];

		// If the page is already present, we don't want to mess with whatever is happening there
		if (cur_pde & PAGE_PRESENT)
			return -1;
	}

	// Mark all the desired pages as 4M, present pages, as well as the custom flags
	unsigned int cur_phys_index;
	for (i = 0; i < size; i++) {
		cur_pde_index = (unsigned int)(start_virt_addr) / (1 << 22) + i;
		cur_phys_index = (unsigned int)(start_phys_addr) / (1 << 22) + i;

		kernel_page_directory[cur_pde_index] = (cur_phys_index * PAGE_TABLE_SIZE * PAGE_ALIGNMENT) |
			flags | PAGE_SIZE_IS_4M | PAGE_PRESENT;
	}

	// Reload the page directory
	write_cr3(&kernel_page_directory);

	return 0;
}

/*
 * Unconditionally unmaps the 4MB aligned region containing the specified region 
 *  from the kernel page directory
 *
 * INPUTS: start_addr: the start virtual address of the region to unmap
 *         size: the size of the region in 4MB increments
 * SIDE EFFECTS: modifies the kernel page directory
 */
void unmap_region(void* start_addr, int size) {
	// The start address rounded down to the nearest 4MB (1 << 22 bytes)
	void* start_addr_aligned = (void*)((unsigned int)start_addr / (1 << 22) * (1 << 22));

	// Mark all the desired pages as not present
	unsigned int i, cur_pde_index;
	for (i = 0; i < size; i++) {
		cur_pde_index = (unsigned int)(start_addr_aligned) / (1 << 22) + i;

		kernel_page_directory[cur_pde_index] = 0;
	}

	// Reload the page directory
	write_cr3(&kernel_page_directory);
}

/*
 * Finds a 4MB aligned region containing the specified region, identity maps it to the kernel page
 *  directory and flushes the TLB
 *
 * INPUTS: start_addr: the start address of the region to map in
 *         size: the size of the region in bytes
 *         flags: flags to assign to the page directory entry (PAGE_PRESENT and PAGE_SIZE_IS_4M added by default)
 * OUTPUTS: 0 if it succeeded or -1 if the region is being used for something else
 *           which includes the case where it's mapped into the kernel but not fully
 */
int identity_map_region(void* start_addr, int size, unsigned int flags) {
	// The start address rounded down to the nearest 4MB (1 << 22 bytes)
	void* start_addr_aligned = (void*)((unsigned int)start_addr / (1 << 22) * (1 << 22));
	// The number of page directory entries to mark as present
	// We take the original # bytes and add on the extra bytes occupied by aligning to 4MB boundary
	//  and then divide by 4MB to get the number of page directory entries
	unsigned int num_pdes = (size + (unsigned int)(start_addr - start_addr_aligned)) / (1 << 22);

	// Identity map the given region
	return map_region(start_addr_aligned, start_addr_aligned, num_pdes, flags);
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
