#include "../include/block.h"

void _mount_disk(int ncyl, int nsec);

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>


#include "limits.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../include/common.h"
#include "../../include/log.h"
#include "stdbool.h"

#define BLOCKSIZE 512

int _ncyl, _nsec, ttd;
char *diskFile;
int fd;
int FILE_SIZE = 0; //n bytes
int lastCyl = 0; // last cylinder accessed

void diskDelay(int c1, int c2){ // simulating the delay between cylinders
    int delay = ttd * abs(c1 - c2);
    usleep(delay);
    if(delay > 0){
        // fprintf(stderr, "Disk delay: %d microseconds\n", delay);        
    }
}

int init_disk(char *filename, int ncyl, int nsec, int _ttd) {
    _ncyl = ncyl;
    _nsec = nsec;
    ttd = _ttd;
    lastCyl = 0;
    FILE_SIZE = ncyl * nsec * BLOCKSIZE;
    fd = open(filename , O_RDWR | O_CREAT, 0644);
    if(fd == -1){
        Log("Error opening/creating file");
        close(fd);
        return -1;
    }
    // open file
    int result = lseek(fd, FILE_SIZE - 1, SEEK_SET); // expand the file to the size of the disk
    if(result == -1){
        Log("error when stretching file with lseek\n");
        close(fd);
        return -1;
    }
    
    result = write(fd, "", 1); // write a byte at the end of the file
    if(result < 0){
        Log("error when writing to file at the begining\n");
        close(fd);
        return -1;
    }
    // stretch the file

    diskFile = (char *)mmap(NULL,FILE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(diskFile == MAP_FAILED){
        Log("error when mapping file to memory\n");
        close(fd);
        return -1;
    }
    // mmap

    Log("Disk initialized: %s, %d Cylinders, %d Sectors per cylinder", filename, ncyl, nsec);
    return 0;
}

// all cmd functions return 0 on success
int disk_cmd_i(int *ncyl, int *nsec) {
    // get the disk info
    *ncyl = _ncyl;
    *nsec = _nsec;
    return 0;
}

int disk_cmd_r(int cyl, int sec, char *buf) {
    // read data from disk, store it in buf
    if (cyl >= _ncyl || sec >= _nsec || cyl < 0 || sec < 0) {
        Log("Invalid cylinder or sector");
        return 1;
    }
    int start = (cyl * _nsec + sec) ;
    if (start*BLOCKSIZE + BLOCKSIZE > FILE_SIZE) {
        Log("Invalid read");
        return 1; //read a block each time
    }
    memcpy(buf, diskFile + start*BLOCKSIZE, BLOCKSIZE);

    diskDelay(lastCyl, cyl); // simulate the delay between cylinders
    lastCyl = cyl; // update the last cylinder accessed

    return 0;
}

int disk_cmd_w(int cyl, int sec, int len, char *data) {
    if (cyl >= _ncyl || sec >= _nsec || cyl < 0 || sec < 0) {
        Log("Invalid cylinder or sector");
        return 1;
    }

    char *buf = (char*)malloc(BLOCKSIZE * sizeof(char));
    if (buf == NULL) {
        Log("Error allocating memory");
        return 1;
    }

    if(len > BLOCKSIZE){
        Log("Data too long");
        free(buf);
        return  1;
    }else{
        memcpy(buf, data, len);
        if(len < BLOCKSIZE){
            memset(buf + len, 0, BLOCKSIZE - len);
            // fill the rest of the buffer with zeros
        }
    }

    int start = cyl * _nsec + sec;
    memcpy(diskFile + start*BLOCKSIZE, buf, BLOCKSIZE);
    free(buf);
    // write data to disk

    diskDelay(lastCyl, cyl); // simulate the delay between cylinders
    lastCyl = cyl; // update the last cylinder accessed
    return 0;
}

void close_disk() {
    close(fd);
    if (diskFile != NULL) {
        munmap(diskFile, FILE_SIZE); // unmap the file
        diskFile = NULL;
    }
    fd = -1; // set fd to -1 to indicate that the file is closed
    FILE_SIZE = 0; // set FILE_SIZE to 0 to indicate that the file is closed
    Log("Disk closed");
    // close the file
}



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
    .root = 0,
    .n_users = 0,
    .users = {0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
};


void _mount_disk(int ncyl, int nsec){
    _ncyl = ncyl;
    _nsec = nsec;
    ttd = 20;
    int res = init_disk("futa.img", ncyl, nsec, ttd);
    if(res < 0){
        fprintf(stderr, "Error initializing disk\n");
        close_disk();
        Error("_mount_disk: error when initializing disk");
        return;
    }

    uchar tmp[BSIZE];
    memset(tmp, 0, BSIZE);
    read_block(0, tmp); // read superblock from disk
    memcpy(&sb, tmp, sizeof(sb)); // copy superblock to sb

    if(sb.size != ncyl * nsec){
        Warn("Disk size mismatch: %d != %d, reinitializing disk", sb.size, ncyl * nsec);
        sb.size = ncyl * nsec;
        sb.bmapstart = 1;
        sb.n_bitmap_blocks = (sb.size / BPB) + 1;
        sb.iNode_start = sb.bmapstart + sb.n_bitmap_blocks; //start point of iNode
        sb.data_start = sb.size / 2; // 50% of the disk for data
        sb.n_blocks = sb.size - sb.data_start; //remaining blocks for data
        sb.n_iNodes = sb.data_start - sb.iNode_start; //remaining blocks for iNodes
        sb.bitmap = (bool *)malloc(sb.n_bitmap_blocks * BSIZE);
        sb.root = 0; //uninitialized root 

        memset(sb.bitmap, 0 , sb.n_bitmap_blocks * BSIZE);
        for(int i = sb.bmapstart; i < sb.bmapstart + sb.n_bitmap_blocks ; i++)
            zero_block(i); //initialize bitmap blocks

        zero_block(0); 

        uchar *buf = (uchar *)malloc(BSIZE);
        memcpy(buf, &sb, sizeof(sb));
        write_block(0, buf);
        free(buf); // write superblock to disk
    }else{
        sb.n_bitmap_blocks = (sb.size / BPB) + 1;
        // if(sb.bitmap != NULL) free(sb.bitmap);
        sb.bitmap = (bool *)malloc(sb.n_bitmap_blocks * BSIZE);
        if(sb.bitmap == NULL){
            Warn("Error allocating memory for bitmap");
            return;
        }


        for(int i = sb.bmapstart; i< sb.bmapstart + sb.n_bitmap_blocks; i++){
            read_block(i, (uchar *)(sb.bitmap + (i - sb.bmapstart) * BSIZE));
        }
        // read bitmap from disk
    }
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
        Warn("allocate block: No free blocks");
        return 0;
    }
    // zero the newly allocated block
    zero_block(b);
    return b;
}

void free_block(uint bno) {

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
    *ncyl = _ncyl;
    *nsec = _nsec;
}

void read_block(int blockno, uchar *buf) {
    if(blockno <0 || blockno >= sb.size){ //sb.size is the total number of blocks
        Warn("read_block: block number out of range");
        return;
    }
    int cyl = blockno / _nsec, sec = blockno % _nsec;
    int res = disk_cmd_r(cyl, sec, (char *)buf); // write data to disk
    if(res < 0){
        Error("read_block: error when reading disk block");
        return ;
    }
    return ;
}

void write_block(int blockno, uchar *buf){
    
    if(blockno <0 || blockno >= sb.size){ //sb.size is the total number of blocks
        Warn("read_block: block number out of range");
        return;
    }

    int cyl = blockno / _nsec, sec = blockno % _nsec;
    int res = disk_cmd_w(cyl, sec, BSIZE, (char *)buf); // write data to disk
    if(res < 0){
        Error("write_block: error writing block");
        return ;
    }
    return ;
}

void exit_block(){
    if(sb.bitmap != NULL){
        free(sb.bitmap);
        sb.bitmap = NULL;
    }
    uchar tmp[BSIZE];
    memset(tmp, 0, BSIZE);
    memcpy(tmp, &sb, sizeof(sb));
    write_block(0, tmp); // write superblock to disk
    // write superblock to disk
    close_disk();
}
