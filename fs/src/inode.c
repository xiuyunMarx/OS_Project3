#include "../include/inode.h"

#include <assert.h>
#include <bits/types/locale_t.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../include/block.h"
#include "../../include/log.h"
#include "../include/common.h"


void copy_to_diNode(dinode *d, inode *ip){
    d->blocks = ip->blocks;
    d->fileSize = ip->fileSize;
    d->linkCount = ip->linkCount;
    d->modTime = ip->modTime;
    d->owner = ip->owner;
    d->permission = ip->permission;
    d->type = ip->type;
    d->inum = ip->inum;
    d->refCount = ip->refCount;
    memcpy(d->name, ip->name, MAXNAME);
    memcpy(d->addrs, ip->addrs, (NDIRECT + 2) * sizeof(uint));
}

void copy_from_diNode(inode *ip, dinode *d){
    ip->blocks = d->blocks;
    ip->fileSize = d->fileSize;
    ip->linkCount = d->linkCount;
    ip->modTime = d->modTime;
    ip->owner = d->owner;
    ip->permission = d->permission;
    ip->type = d->type;
    ip->inum = d->inum;
    ip->refCount = d->refCount;
    memcpy(ip->name, d->name, MAXNAME);
    memcpy(ip->addrs, d->addrs, (NDIRECT + 2) * sizeof(uint));
}

void store_iNode(inode *ip){
    if(ip == NULL){
        Error("store_iNode: ip is NULL");
        return;
    }

    uchar *tmp = (uchar *)malloc(BSIZE);
    memset(tmp, 0, BSIZE);
    dinode *d = (dinode *)tmp;
    copy_to_diNode(d, ip);
    write_block(ip->inum, tmp);
    free(tmp);
}

bool _is_exist(uint inum){ //检查在inum的位置是否有inode
    if(inum == 0){
        Error("is_exist: inum is 0");
        return false;
    }
    if(inum < sb.iNode_start ||inum >=sb.data_start){
        Error("is_exist: inum %d is out of range", inum);
        return false;
    }

    uchar *bm = (uchar *)sb.bitmap;
    uint byte = inum / 8, bit = inum % 8;
    if((bm[byte] & (1u << bit)) == 0) { //check if this bit is 0
        Error("is_exist: the block %d is not allocated", inum);
        return false;
    }else{
        return true;
    }
}

inode *iget(uint inum) {
    if(_is_exist(inum) == false){
        Error("iget: iNode %d does not exist", inum);
        return NULL;
    }

    inode *ret = (inode *)malloc(sizeof(inode));
    uchar *tmp = (uchar *)malloc(BSIZE);
    memset(tmp, 0, BSIZE);
    read_block(inum, tmp);
    dinode *d = (dinode *)tmp;
    copy_from_diNode(ret, d);
    free(tmp);
    return ret;
}

void iput(inode *ip) {
    iupdate(ip);
    free(ip);
}

inode *ialloc(short type) {
    inode *ret = (inode *)malloc(sizeof(inode));
    ret->type = type;
    ret->fileSize = 0;
    ret->blocks = 0;
    memset(ret->addrs, 0 , (NDIRECT + 2) * sizeof(uint));
    uint inum = allocate_iNode_block();
    if(inum == 0){
        free(ret);
        Error("ialloc: no enough space");
        return NULL;
    }
    ret->inum = inum;
    store_iNode(ret);
    return ret;
}

void iupdate(inode *ip) {
    if(ip == NULL){
        Error("iupdate: ip is NULL");
        return;
    }
    ip->modTime = time(NULL);
    store_iNode(ip);
}

uint _which_read(inode *ip ,uint logic){
    const uint links_per_block = BSIZE / sizeof(uint);
    if(logic < NDIRECT){
        assert(ip->addrs[logic] != 0);
        return ip->addrs[logic];
    }else if(NDIRECT <= logic && logic < NDIRECT + links_per_block){
        assert(ip->addrs[NDIRECT] != 0);
        uint *single_indirect = (uint *)malloc(BSIZE);
        read_block(ip->addrs[NDIRECT], (uchar *)single_indirect);
        uint ret = single_indirect[logic - NDIRECT];
        free(single_indirect);
        return ret;
    }else if(logic < NDIRECT + links_per_block + links_per_block * links_per_block){
        assert(ip->addrs[NDIRECT + 1] != 0); //level 0 exists
        uint *double_indirect0 = (uint *)malloc(BSIZE);
        read_block(ip->addrs[NDIRECT + 1], (uchar *)double_indirect0);

        uint which = (logic - NDIRECT - links_per_block) / links_per_block;
        uint offset = (logic - NDIRECT - links_per_block) % links_per_block; 
        //which block in the second level settled in , and offset is the index in the block

        assert(double_indirect0[which] != 0); //level 1 exists
        uint *double_indirect1 = (uint *)malloc(BSIZE);
        read_block(double_indirect0[which], (uchar *)double_indirect1);
        uint ret = double_indirect1[offset];

        free(double_indirect0);
        free(double_indirect1);
        return ret;
    }else {
        Error("_which_read: logic is out of range");
        return 0;
    }
}


int readi(inode *ip, uchar *dst, uint off, uint n) {
    if(n == 0){
        return 0; // No data to read
    }
    // Read from an inode (returns bytes read or -1 on error)
    // the bytes beyond the file size will be ignored
    if (off >= ip->fileSize) {
        return 0; // No data to read
    }
    int bytesRead = 0;
    uint start_block = off / BSIZE;
    uint end_block = min(ip->fileSize - 1 , off + n - 1) / BSIZE; // Calculate the end block, is this correct?
    uchar *fileSlot = (uchar *)malloc((end_block - start_block + 1) * BSIZE);
    memset(fileSlot, 0, (end_block - start_block + 1) * BSIZE);

    for(uint logic = start_block; logic <= end_block; logic ++){
        uint bno = _which_read(ip, logic);
        read_block(bno, fileSlot + (logic - start_block) * BSIZE);
    }
    uint left = off % BSIZE; //where the data wanted starts in fileSlot
    bytesRead = min(n, ip->fileSize - off); // the bytes to be read
    memcpy(dst, fileSlot + left, bytesRead);
    free(fileSlot);
    return bytesRead;
}

uint _which_write(inode *ip, uint logic){ //this function should be called sequentially
    //return the block number of block that waits to be written
    //allocate a new block if the block is not allocated
    const uint links_per_block = BSIZE / sizeof(uint);
    if(logic < NDIRECT){
        if(ip->addrs[logic] == 0){
            uint bno = allocate_data_block();
            if(bno == 0){
                Error("_which_write: no enough space");
                return 0;
            }else{
                ip->addrs[logic] = bno;
                ip->blocks++;
            }
        }
        assert(ip->addrs[logic] != 0);
        iupdate(ip);
        return ip->addrs[logic];
    }else if(NDIRECT <= logic && logic <NDIRECT + links_per_block){
        if(ip->addrs[NDIRECT] == 0){ // the block for single indirect is not allocated yet
            uint bno = allocate_data_block();
            if(bno == 0){
                Error("_which_write: no enough space");
                return 0;
            }else{
                ip->addrs[NDIRECT] = bno;
                //these are index blocks , no need to increase ip->blocks
            }
        }
        assert(ip->addrs[NDIRECT] != 0);

        uint *single_indirect = (uint *)malloc(BSIZE);
        read_block(ip->addrs[NDIRECT], (uchar *)single_indirect);
        if(single_indirect[logic - NDIRECT] == 0){
            uint bno = allocate_data_block();
            if(!bno){
                Error("_which_write: no enough space");
                free(single_indirect);
                return 0;
            }
            single_indirect[logic - NDIRECT] = bno;
            ip->blocks++; //a data block is allocated
        }
        assert(single_indirect[logic - NDIRECT] != 0);
        write_block(ip->addrs[NDIRECT], (uchar *)single_indirect);
        uint ret = single_indirect[logic - NDIRECT];
        free(single_indirect);
        iupdate(ip);
        return ret;
    }else if(logic < NDIRECT + links_per_block + links_per_block * links_per_block){
        if(ip->addrs[NDIRECT + 1] == 0){ // 0th level indirect block is not allocated yet
            uint bno = allocate_data_block();
            if(bno == 0){
                Error("_which_write: no enough space");
                return 0;
            }else{
                ip->addrs[NDIRECT + 1] = bno;
                //these are index blocks , no need to increase ip->blocks
            }
        }
        assert(ip->addrs[NDIRECT + 1] != 0); //level 0 exists

        uint where = (logic - NDIRECT - links_per_block) / links_per_block; //which second level block
        uint offset = (logic - NDIRECT - links_per_block) % links_per_block;

        uint *double_indirect0 = (uint *)malloc(BSIZE);
        read_block(ip->addrs[NDIRECT + 1], (uchar *)double_indirect0);

        if(double_indirect0[where] == 0){ // the corresponding second level block is not allocated 
            uint bno = allocate_data_block();
            if(bno == 0){
                Error("_which_write: no enough space");
                free(double_indirect0);
                return 0;
            }else{
                double_indirect0[where] = bno;
                //these are index blocks , no need to increase ip->blocks
            }
        }
        assert(double_indirect0[where] != 0); //level 1 exists

        uint *double_indirect1 = (uint *)malloc(BSIZE);
        read_block(double_indirect0[where],(uchar *)double_indirect1);
        if(double_indirect1[offset] == 0){ // the data block doesn't exist
            uint bno = allocate_data_block();
            if(bno == 0){
                Error("_which_write: no enough space");
                free(double_indirect0);
                free(double_indirect1);
                return 0;
            }else{
                double_indirect1[offset] = bno;
                ip->blocks++;
            }
        }
        assert(double_indirect1[offset]!=0); //data block exists

        write_block(ip->addrs[NDIRECT + 1],(uchar *)double_indirect0);
        write_block(double_indirect0[where], (uchar *)double_indirect1);
        iupdate(ip);

        uint ret = double_indirect1[offset];
        free(double_indirect0);
        free(double_indirect1);
        return ret;
    }else{
        Error("_which_write: logic block number out of range");
        return 0;
    }
}

int writei(inode *ip, uchar *src, uint off, uint n) {
    if(off > ip->fileSize){
        Error("writei: off too large, file size is %d, off is %d", ip->fileSize, off);
        return -1;
    }
    uint start_block = off / BSIZE, end_block = (off + n - 1) / BSIZE;
    uchar *toWrite  = (uchar *)malloc((end_block - start_block + 1) * BSIZE);
    memset(toWrite, 0, (end_block - start_block + 1) * BSIZE);


    //add prefix for src to make it fit in integer numbers of blocks
    uchar *tmp = (uchar *)malloc(BSIZE);
    readi(ip, tmp, start_block * BSIZE, BSIZE);
    memcpy(toWrite, tmp, off % BSIZE);
    memcpy(toWrite + off % BSIZE, src, n);


    //add suffix for src to make it fit in integer numbers of blocks
    uint stop_point = (off + n - 1)%BSIZE;
    readi(ip, tmp, end_block * BSIZE, BSIZE);
    memcpy(toWrite + off % BSIZE + n, tmp + stop_point + 1, BSIZE - stop_point - 1);
    free(tmp);

    for(uint logic = start_block ; logic <= end_block;logic++){
        uint bno = _which_write(ip, logic);
        if(bno == 0){
            Error("writei: no enough space");
            free(toWrite);
            return -1;
        }
        write_block(bno, toWrite + (logic - start_block) * BSIZE);
    }

    free(toWrite);
    ip->fileSize = max(ip->fileSize, off + n);
    iupdate(ip);
    return n;
}


int wipeout_inode(inode *ip){
    if(ip == NULL){
        Error("wipeout_inode: ip is NULL");
        return E_ERROR;
    }
    uint total_blocks = ip->blocks;
    for(uint i=0;i<total_blocks;i++){
        uint bno = _which_read(ip, i);
        free_block(bno);
    }

    if(ip->addrs[NDIRECT] != 0){
        free_block(ip->addrs[NDIRECT]);
    }

    if(ip->addrs[NDIRECT + 1] != 0){
        uint *double_indirect0 = (uint *)malloc(BSIZE);
        read_block(ip->addrs[NDIRECT + 1], (uchar *)double_indirect0);
        for(uint i=0;i<BSIZE/sizeof(uint);i++){
            if(double_indirect0[i] != 0){
                free_block(double_indirect0[i]);
            }
        }
        free_block(ip->addrs[NDIRECT + 1]);
        free(double_indirect0);
    }

    free_block(ip->inum);
    return E_SUCCESS;
}
