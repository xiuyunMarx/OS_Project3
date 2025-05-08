#ifndef __BLOCK_H__
#define __BLOCK_H__

#include "common.h"
#include "stdbool.h"

typedef struct {
    uint magic;      // Magic number, used to identify the file system
    uint size;       // Size in blocks
    uint bmapstart;  // Block number of first free map block
    bool *bitmap;  // Bitmap of free blocks
    uint n_blocks;
    uint n_iNodes;
    uint data_start;  // Block number of first data block
    uint iNode_start;  // Block number of first inode block
    uint n_bitmap_blocks; // Number of blocks for the bitmap
    uint root; // Root inode number
} superblock;

// sb is defined in block.c
extern superblock sb;

void zero_block(uint bno);
uint allocate_block();
void free_block(uint bno);

void get_disk_info(int *ncyl, int *nsec);
void read_block(int blockno, uchar *buf);
void write_block(int blockno, uchar *buf);

uint allocate_data_block();
uint allocate_iNode_block();

void _mount_disk();
bool hasDisk();
#endif