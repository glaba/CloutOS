#ifndef _FILE_SYSTEM_H
#define _FILE_SYSTEM_H

#include "types.h"
#include "multiboot.h"
#include "lib.h"

#define BLOCK_SIZE 4096 /* kilobytes */
#define NUM_INODES 63
#define CHARS_PER_BLOCK 4096 /* 4kB block */
#define BLOCKS_PER_INODE 1023
#define RTC_FTYPE	0
#define DIR_FTYPE	1
#define FILE_FTYPE	2
#define TERM_FTYPE	3
#define FNAME_LEN 32
#define BYTES_DENTRY 64

typedef struct dentry {
	int8_t fname[FNAME_LEN];
	int32_t ftype; //0-RTC, 1-Directory, 2-Regular File
	int32_t inode;
	int32_t pad[6];
} dentry_t;

typedef struct bootblock {
	int32_t dir_entries_cnt;
	int32_t inode_cnt;
	int32_t data_block_cnt;
	int32_t pad[13];
	dentry_t dentry[NUM_INODES];
} bootblock_t;

typedef struct inode {
	int32_t length;
	int32_t data_block[BLOCKS_PER_INODE]; /* number based on lecture slides */
} inode_t;

typedef struct data_block {
	uint8_t data[CHARS_PER_BLOCK];
} data_block_t;

typedef struct {
	int32_t (*read) (int32_t fd, void* buf, int32_t nbytes);
	int32_t (*write) (int32_t fd, const void* buf, int32_t nbytes);
	int32_t (*open) (const uint8_t* filename);
	int32_t (*close) (int32_t fd);
} fops_t;


int32_t read_dentry_by_name(const uint8 t* fname, dentry_t* dentry);
int32_t read_dentry_by_index(uint32_t index, dentry_t* dentry);
int32_t read_data(uint32_t inode, uint32_t offset, uint8_t* buf, uint32_t length);
uint32_t read_directory(uint32_t offset, uint8_t* buf, uint32_t length);
uint32_t read_directory_entry(uint32_t dir_entry, uint8_t* buf, uint32_t length);
inode_t * get_inode_ptr(uint32_t inode);
int32_t load(dentry_t * d, uint8_t * mem);

#endif
