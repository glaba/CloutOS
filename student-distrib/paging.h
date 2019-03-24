#ifndef _PAGING_H
#define _PAGING_H

#include "lib.h"

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
#define KERNEL_START_ADDR      0x400000
// The kernel heap starts at 8MB in physical memory
#define KERNEL_HEAP_START_ADDR 0x800000

/////////////////////////////////////////////////
// Page table / page directory entry constants //
/////////////////////////////////////////////////
// Enabled if the corresponding page is fixed between processes and TLB should not flush 
#define PAGE_GLOBAL             0x100
// Enabled if the page size is 4 MiB rather than 4 KiB
#define PAGE_SIZE_IS_4M         0x80
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

// Unconditionally unmaps the 4MB aligned region containing the specified region
void unmap_region_from_kernel(void* start_addr, int size);

// Finds a 4MB aligned region containing the specified region, adds it to the kernel page
//  directory and flushes the TLB
int map_region_into_kernel(void* start_addr, int size, unsigned int flags);

#endif /* _PAGING_H */
