#ifndef _PROC_H
#define _PROC_H

#include "file_system.h"
#include "types.h"

//max number of files is 8
#define FILE_ARRAY_LEN	8
//mask to get process control block
#define PCB_MASK        0xFFFFE000
//mask to get arguments
#define ARGS_MAX        128
//where it is in vidmem
#define PROG_VIDMEM_ADDR 0x8400000
//max prucesses running at once
#define MAX_PROCESSES   6

#define MAX_DEVICES 6

#define MAX_TERMINALS	3

/* fd flags */
#define FD_LIVE            0x1

/* context struct
 * Used in pcb_t struct. Contains important
 * register values to remember for context switching.
 */
typedef struct {
    uint32_t esp, eip, esp0, ebp;
} context_t;

/* fd struct
 * Used in pcb_t struct. Contains important
 * information about a open file used in the user
 * program.
 */
typedef struct {
    fops_t * fops;
    inode_t * inode;
    uint32_t inode_num;
    uint32_t pos;
    uint32_t flags;
} fd_t;

/* pcb struct
 * Contains important values for each process
 * to help with context switching.
 */
typedef struct pcb {
	fd_t files[FILE_ARRAY_LEN];
    uint8_t args[ARGS_MAX];
    uint32_t args_len;
	int32_t pid;
	struct pcb* parent_pcb;
    context_t context;
    uint32_t esp_parent, ebp_parent;
    uint32_t * pd;
    int32_t term_num;
} pcb_t;

/* Registers a device by adding it to the 'devices' array of fops_t*.
 * Once a device/file is registered, it can be used by a user program. */
int add_device(uint32_t fytpe, fops_t * fops);

/* Obtains an 'fops' pointer based on the ftype. */
fops_t * get_device_fops(uint32_t fytpe);

/* adds a process to the 'procs' bitmap array. */
int32_t add_process();

/* deletes a process if the 'procs' array. */
int32_t delete_process(int32_t pid);

/* gets a pointer to the page directory corresponding
 * to the pid of the specified process. */
uint32_t * get_process_pd(int32_t pid);

/* indicates if theres enough space in memory to add a new process */
int32_t processes();

/* gets the pid of the active process running on terminal 'term_num' */
int32_t get_active_process(uint32_t term_num);

/* sets the active_processes array at index'term_num' with the process id.*/
int32_t set_active_process(uint32_t term_num, int32_t pid);

/* indicates if theres enough space in memory to add a new process */
int32_t free_procs();

/* macro to get the current ebp */
#define get_ebp(x)          \
do {                        \
    asm volatile (          \
        "movl %%ebp, %0"    \
        : "=m" (x)          \
    );                      \
} while(0)

/* macro to get the  current esp */
#define get_esp(x)          \
do {                        \
    asm volatile (          \
        "movl %%esp, %0"    \
        : "=m" (x)          \
    );                      \
} while(0)

/* macro to get the pointer to the pbc that the current
 * esp is pointing to.
 */
#define pcb(x)                          \
do {                                    \
    uint32_t esp;                       \
    get_esp(esp);                       \
    x = (pcb_t *) (esp & PCB_MASK);     \
} while(0)

#endif
