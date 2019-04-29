#ifndef _PAGING_H
#define _PAGING_H

#include "lib.h"
#include "types.h"

// The last address we will deem as accessible in the system 
// This is hardcoded to 224MB, which is a conservative estimate, since accessing the real value
//  is a fairly non trivial task that involves messing around with the BIOS
#define LAST_ACCESSIBLE_ADDR 0xE000000

// The size of a large 4MB page (4MB)
#define LARGE_PAGE_SIZE 0x400000
// The size of a normal 4KB page (4KB)
#define NORMAL_PAGE_SIZE 0x1000

// The number of page directory entries in a page directory (each is 4 bytes)
#define PAGE_DIRECTORY_SIZE 1024
// The number of page table entries in a page table (each is 4 bytes)
#define PAGE_TABLE_SIZE     1024
// The alignment of page tables and pages in memory (they must be 4 KiB aligned)
#define PAGE_ALIGNMENT      4096

// The size of a regular 4 KiB page
#define PAGE_SIZE           0x001000
// The size of a 4 MiB page
#define LARGE_PAGE_SIZE     0x400000

// The kernel starts at 4MB in physical memory
#define KERNEL_START_ADDR        0x400000
// The size of a kernel stack
#define KERNEL_STACK_SIZE        0x2000
// The kernel heap starts at 8MB in physical memory
#define KERNEL_HEAP_START_ADDR   0x800000
// The kernel heap ends at 20MB in physical memory
#define KERNEL_HEAP_END_ADDR     0x1400000
// The size of the kernel heap (12MB)
#define HEAP_SIZE (KERNEL_HEAP_END_ADDR - KERNEL_HEAP_START_ADDR)
// The kernel ends with the kernel heap at 20MB
#define KERNEL_END_ADDR KERNEL_HEAP_END_ADDR
// The virtual address that video memory is mapped to for userspace programs (192MB)
#define VIDEO_USER_VIRT_ADDR (192 * 1024 * 1024)

/////////////////////////////////////////////////
// Page table / page directory entry constants //
/////////////////////////////////////////////////
// Enabled if the corresponding page is fixed between processes and TLB should not flush
#define PAGE_GLOBAL              0x100
// Enabled if the page size is 4 MiB rather than 4 KiB
#define PAGE_SIZE_IS_4M          0x80
// Enabled if the memory should not be cached; program code and data pages should be cached, memory-mapped I/O should not be
#define PAGE_DISABLE_CACHE       0x10
// Enabled if the cache should be a write-through cache, otherwise it is write-back
#define PAGE_WRITE_THROUGH_CACHE 0x8
// Enabled if the memory should be accessible by privilege levels 0-3, otherwise only privilege levels 0-2 can access
#define PAGE_USER_LEVEL          0x4
// Enabled if the mapped memory is read/write (otherwise, it is read-only)
#define PAGE_READ_WRITE          0x2
// Enabled if the page is present
#define PAGE_PRESENT             0x1

// Initializes paging by setting Page Directory Base Register to page directory
//  that maps the 4M kernel page as well as video memory
void init_paging();

// Unconditionally maps a region of specified size starting from the
//  given virtual address to the region of the same size starting from the given physical address
int32_t map_region(void *start_phys_addr, void *start_virt_addr, uint32_t num_pdes, uint32_t flags);

// Unconditionally unmaps the 4MB aligned region containing the specified region
void unmap_region(void* start_addr, uint32_t num_pdes);

// Unconditionally maps in a 4MB-aligned region made up of large 4MB pages
//  that fully contains the desired region
int32_t map_containing_region(void *start_phys_addr, void *start_virt_addr, uint32_t size, uint32_t flags);

// Unconditionally unmaps the smallest 4MB-aligned region made up of large 4MB pages containing the given region
void unmap_containing_region(void *start_addr, uint32_t size);

// Identity maps the smallest 4MB aligned region containing the provided region with the given PDE flags
int32_t identity_map_containing_region(void* start_addr, uint32_t size, unsigned int flags);

// Maps the virtual region starting at 192MB into the provided physical start address with size VIDEO_SIZE
//  so that a userspace program can access it
int32_t map_video_mem_user(void *phys_addr);

// Unmaps the video memory paged in for userspace programs 
void unmap_video_mem_user();

// Returns the index of an unused 4MB page in physical memory and marks it used
int32_t get_open_page();
// Marks the page at the provided index as unused
void free_page(int32_t index);

#endif /* _PAGING_H */
