#ifndef _PAGING_H
#define _PAGING_H

#include "lib.h"

// The number of page directory entries in a page directory (each is 4 bytes)
#define PAGE_DIRECTORY_SIZE 1024
// The number of page table entries in a page table (each is 4 bytes)
#define PAGE_TABLE_SIZE     1024 
// The alignment of page tables and pages in memory (they must be 4 KiB aligned)
#define PAGE_ALIGNMENT      4096

// The kernel starts at 4MB in physical memory
#define KERNEL_START_ADDR   0x400000

/////////////////////////////////////////////////
// Page table / page directory entry constants //
/////////////////////////////////////////////////
// Enabled if the corresponding page is fixed between processes and TLB should not flush 
#define PAGE_GLOBAL             0x100
// Enabled if the page size is 4 MiB rather than 4 KiB
#define PAGE_SIZE_IS_4M         0x80
// Enabled if the memory should be cached, program code and data pages should be cached, memory-mapped I/O should not be
#define PAGE_CACHE              0x10
// Enabled if the memory should be accessible by privilege levels 0-3, otherwise only privilege levels 0-2 can access
#define PAGE_USER_LEVEL         0x4
// Enabled if the mapped memory is read/write (otherwise, it is read-only)
#define PAGE_READ_WRITE         0x2
// Enabled if the page is present
#define PAGE_PRESENT            0x1

// Initializes paging by setting Page Directory Base Register to page directory
//  that maps the 4M kernel page as well as video memory
void init_paging();

#endif /* _PAGING_H */