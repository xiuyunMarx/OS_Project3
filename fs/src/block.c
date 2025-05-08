#include "../include/block.h"

#include "inode.h"
#include "limits.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../include/common.h"
#include "../../include/log.h"
#include "stdbool.h"
superblock sb={
    .magic = 0x12345678,
    .size = 2048,  // 2048 blocks
    .bmapstart = 1,
    .bitmap = NULL,
    .n_blocks = 1024,
    .n_iNodes = 0,
    .data_start = 1024,
    .iNode_start = 2,
    .n_bitmap_blocks = 0,
    .root = 0
};

char *disk = NULL;

void _mount_disk(){
    Warn("mounting disk...: %d", time(NULL));
    disk = (char *)malloc(sb.size * BSIZE);
    sb.n_bitmap_blocks = (sb.size / BPB) + 1;
    sb.iNode_start = sb.bmapstart + sb.n_bitmap_blocks; //start point of iNode
    sb.data_start = sb.size / 2; // 50% of the disk for data
    sb.n_blocks = sb.size - sb.iNode_start; //remaining blocks for data
    sb.n_iNodes = sb.data_start - sb.iNode_start; //remaining blocks for iNodes
    sb.bitmap = (bool *)malloc(sb.n_bitmap_blocks * BSIZE);
    sb.root = 0; //uninitialized
    memset(sb.bitmap, 0 , sb.n_bitmap_blocks * BSIZE);
    for(int i = sb.bmapstart; i < sb.bmapstart + sb.n_bitmap_blocks ; i++)
        zero_block(i); //initialize bitmap blocks

    uchar *buf = (uchar *)malloc(BSIZE);
    memcpy(buf, &sb, sizeof(sb));
    write_block(0, buf);
    free(buf); // write superblock to disk
}

void _fetch_bitmap(){
    for(int i = 0; i < sb.n_bitmap_blocks; i++){
        read_block(sb.bmapstart + i, (uchar *)(sb.bitmap + i * BSIZE));
    }
}

void _update_bitmap(){
    for(int i = 0; i < sb.n_bitmap_blocks; i++){
        write_block(sb.bmapstart + i, (uchar *)(sb.bitmap + i * BSIZE));
    }
}
  

void zero_block(uint bno) {
    uchar buf[BSIZE];
    memset(buf, 0, BSIZE);
    write_block(bno, buf);
}

uint allocate_iNode_block(){ //为一个iNode分配一个块
    if(disk == NULL){
        Warn("initializing disk..., " );
        _mount_disk();
    }

    _fetch_bitmap();
    uchar *bm = (uchar *)sb.bitmap;
    uint b;
    for(b=sb.iNode_start ; b < sb.data_start ; b++){
        uint byte = b / 8;
        uint bit = b % 8;
        if((bm[byte] & (1u << bit)) == 0) { //check if this bit is 0
            // mark bit used
            bm[byte] |= (1u << bit);
            break;
        }
    }
    _update_bitmap();

    if(b>=sb.data_start){
        Warn("allocate inode: No free inodes blocks");
        return 0;
    }

    zero_block(b);
    return b;
}

uint allocate_data_block(){
    if(disk == NULL){
        Warn("initializing disk...");
        _mount_disk();
    }

    _fetch_bitmap();
    uchar *bm = (uchar *)sb.bitmap;
    uint b;
    for(b=sb.data_start ; b < sb.size ; b++){
        uint byte = b / 8;
        uint bit = b % 8;
        if((bm[byte] & (1u << bit)) == 0) { //check if this bit is 0
            // mark bit used
            bm[byte] |= (1u << bit);
            break;
        }
    }
    _update_bitmap();

    if(b>=sb.size){
        Warn("allocate data: No free data blocks");
        return 0;
    }

    zero_block(b);
    return b;
}

uint allocate_block() {
    if(disk == NULL){
        Warn("initializing disk...");
        _mount_disk();
    }

    _fetch_bitmap();
    // bitmap is raw bytes: treat sb.bitmap memory as byte array
    uchar *bm = (uchar *)sb.bitmap;
    uint b;
    for(b = 0; b < sb.size; b++) {
        uint byte = b / 8;
        uint bit = b % 8;
        if((bm[byte] & (1u << bit)) == 0) { //check if this bit is 0
            // mark bit used
            bm[byte] |= (1u << bit);
            break;
        }
    }
    _update_bitmap();
    if(b >= sb.size) {
        warn("allocate block: No free blocks");
        return 0;
    }
    // zero the newly allocated block
    zero_block(b);
    return b;
}

void free_block(uint bno) {
    if(disk == NULL){
        Warn("initializing disk...");
        _mount_disk();
    }

    _fetch_bitmap();
    // clear data block
    zero_block(bno);
    // clear the bit in bitmap
    uchar *bm = (uchar *)sb.bitmap;
    if(bno < sb.size) {
        uint byte = bno / 8;
        uint bit = bno % 8;
        bm[byte] &= ~(1u << bit);
    } else {
        Warn("free block: block number out of range");
    }
    _update_bitmap();
}

void get_disk_info(int *ncyl, int *nsec) {
    *ncyl = *nsec = 10;
}

void read_block(int blockno, uchar *buf) {
    if(disk == NULL){
        Warn("initializing disk...");
        _mount_disk();
    }
    
    if(blockno <0 || blockno >= sb.size){ //sb.size is the total number of blocks
        Warn("read_block: block number out of range");
        return;
    }

    memcpy(buf, disk + blockno * BSIZE, BSIZE);
}

void write_block(int blockno, uchar *buf) {
    if(disk == NULL){
        Warn("initializing disk...");
        _mount_disk();
    }
    
    if(blockno <0 || blockno >= sb.size){ //sb.size is the total number of blocks
        Warn("read_block: block number out of range");
        return;
    }

    memcpy(disk + blockno * BSIZE, buf, BSIZE);
}
