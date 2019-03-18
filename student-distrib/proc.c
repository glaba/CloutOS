#include "proc.h"
#include "lib.h"
#include "paging.h"
#include "x86_desc.h"

static uint32_t proc_count = 0;
static uint8_t procs[MAX_PROCESSES] = {0};
static uint32_t pd[MAX_PROCESSES][PAGE_TABLE_SIZE] __attribute__((aligned (PAGE_ALIGNMENT)));
static int32_t active_processes[MAX_TERMINALS] = {-1, -1, -1};
static fops_t * devices[MAX_DEVICES];

/*
* PURPOSE: Registers a device/file by adding it to the devices array of fops_t*
*          Device/File can be used after it is registered
* INPUTS: ftype - specify the type of device/file to register.
* 			  fops - pointer to an fops corresponding to the ftype.
* SIDE EFFECTS: fills in the devices array at the 'ftype' index with the correct
*               fops pointer
* RETURN VALUE: 0 for success, -1 for failure
*/
int add_device(uint32_t fytpe, fops_t * fops){
  if(ftype < 0 || ftype >= MAX_DEVICES)
    return -1;
  devices[ftype] = fops;
  return 0;
}

/*
* PURPOSE: Gets the fops pointer for a device based on ftype
* INPUTS: Ftype - specify the type of device/file to get
* SIDE EFFECTS: none
* RETURN VALUE: the fops pointer based on ftype passed in
*/
fops_t * get_device_fops(uint32_t fytpe){
  if(ftype < 0 || ftype >= MAX_DEVICES)
    return NULL;
  return devices[ftype];
}

/*
* PURPOSE: Adds a process to 'procs' bitmap array
* INPUTS: none
* SIDE EFFECTS: adds the pid of the new process to the 'procs' array
* RETURN VALUE: the process id num or -1 for failure
*/
int32_t add_process(){
  uint32_t i = 0;
  for(i = 0; i < MAX_PROCESSES; i++){
    if(procs[i] == 0){
      procs[i] = 1;
			proc_count++;
			return i + 1;
    }
  }
  return -1;
}

/*
* PURPOSE: Deletes a process from 'procs' array
* INPUTS: pid - the process id to be deleted
* SIDE EFFECTS: deletes the process with the specified pid from 'procs'
* RETURN VALUE: 0 on success, -1 on failure
*/
int32_t delete_process(int32_t pid);{
  //since the pid is 0 indexed we have to subtract 1 from it
  pid--;
  if(pid < 0 || pid >= MAX_PROCESSES)
    return -1;

  procs[pid] = 0;
	proc_count--;
	return 0;
}

/*
* PURPOSE: Returns a pointer to the page directory corresponding to the pid of
*          the specified process
* INPUTS: pid - the process id of the page directory to get
* SIDE EFFECTS: none
* RETURN VALUE: pointer to the page directory
*/
uint32_t * get_process_pd(int32_t pid){
  //since the pid is 0 indexed we have to subtract 1 from it
  pid--;
	if(pid < 0 || pid >= MAX_PROCESSES)
		return NULL;

	return pd[pid];
}

/*
* PURPOSE: Returns the total number of processes running
* INPUTS: none
* SIDE EFFECTS: none
* RETURN VALUE: proc_count - the total number of precesses running
*/
int32_t processes(){
  return proc_count;
}

/*
* PURPOSE: Returns the pid of the active process running on terminal 'term_num'
* INPUTS: term_num - terminal number to get the active process on
* SIDE EFFECTS: none
* RETURN VALUE: the pid of the active process in the specified terminal
*/
int32_t get_active_process(uint32_t term_num){
	if(term_num < 0 || term_num >= MAX_TERMINALS)
    return -1;

	return active_processes[term_num];
}

/*
 * PURPOSE: sets the active_processes array at index
 *				  'term_num' with the process id.
 * INPUTS: term_num - index in the active_process array
 *			   pid - process id indicating the active process
 * SIDE EFFECTS: none
 * RETURN VALUE: 0 on success, -1 on failure.
 */
int32_t set_active_process(uint32_t term_num, int32_t pid){
	if(term_num < 0 || term_num >= MAX_TERMINALS)
    return -1;

	active_processes[term_num] = pid;
	return 0;
}

/*
 * PURPOSE: indicates if theres enough space in memory to  add a new process
 * INPUTS: none
 * OUTPUTS: none
 * RETURN VALUE: 1 if total processes is less than the max amount of processes
 * allowed (6). 0 if another process cannot be added.
 */
int32_t free_procs() {
	return proc_count < MAX_PROCESSES;
}
