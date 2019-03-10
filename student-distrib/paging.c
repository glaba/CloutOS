#include "paging.h"

static unsigned int kernel_page_directory[PAGE_DIRECTORY_SIZE];
static unsigned int video_page_table[PAGE_TABLE_SIZE] __attribute__((aligned (PAGE_ALIGNMENT)));

/*
 * Writes a page directory address to the cr3 register
 */
#define write_cr3(pd_addr)            \
do {                                  \
	asm volatile ("movl %k0, %%cr3"   \
		:                             \
		: "r"((pd_addr))              \
		: "cc"                 \
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
 * Initializes paging by setting the Page Directory Base Register (PDBR / CR3)
 *  to point to the kernel page directory 
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
