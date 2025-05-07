#ifndef __INODE_H__
#define __INODE_H__

#include "common.h"
#include "block.h"
#define MAXNAME 12
#define NDIRECT 10  // Direct blocks, you can change this value
#define MAXFILEB (NDIRECT + APB + APB * APB)
enum {
    T_DIR = 1,   // Directory
    T_FILE = 2,  // File
};


// You should add more fields, this is format of iNode that stored in disk
// the size of a dinode must divide BSIZE
typedef struct {
    char name[MAXNAME]; // File name
    uint inum; //这个inode 块在磁盘中的绝对坐标
    ushort type:2;              // File type
    ushort permission:4;  //file Mode
    ushort owner:15;
    uint fileSize;                // Size in bytes
    uint blocks;              // Number of blocks, may be larger than size
    uint addrs[NDIRECT + 2];  // Data block addresses,
                              // the last two are indirect blocks, single and double indirect
    uint refCount;  // Reference count, for current file in iNode
    uint linkCount; // Number of links to file
    uint modTime; // Modification time
} dinode;

// inode in memory
// more useful fields can be added, e.g. reference count
typedef struct {
    char name[MAXNAME]; // File name
    uint inum; //the index of this inode, which is the absolute number in disk
    ushort type:2;              // File type
    ushort permission:4;
    ushort owner:15;
    uint fileSize;                // Size in bytes
    uint blocks;              // Number of blocks, may be larger than size
    uint addrs[NDIRECT + 2];  // Data block addresses,
                              // the last two are indirect blocks, single and double indirect
    uint refCount;  // Reference count, for current file in iNode
    uint linkCount; // Number of links to file, when this iNode serves as directory
    uint modTime; // Modification time
} inode;



// You can change the size of MAXNAME
#define MAXNAME 12

// Get an inode by number (returns allocated inode or NULL)
// Don't forget to use iput()
inode *iget(uint inum);

// Free an inode (or decrement reference count), free a memory inode
void iput(inode *ip);

// Allocate a new inode of specified type (returns allocated inode or NULL)
// Don't forget to use iput()
inode *ialloc(short type);

// Update disk inode with memory inode contents
void iupdate(inode *ip);

// Read from an inode (returns bytes read or -1 on error)
int readi(inode *ip, uchar *dst, uint off, uint n);

// Write to an inode (returns bytes written or -1 on error)
int writei(inode *ip, uchar *src, uint off, uint n);

void copy_to_diNode(dinode *d, inode *ip);

inline uint maxFileSize(){
    uint ret = 0;
    ret += NDIRECT * BSIZE;
    ret += BSIZE * (BSIZE / sizeof(uint)); //single indirect
    ret += BSIZE * (BSIZE / sizeof(uint)) * (BSIZE / sizeof(uint)); //double indirect
    return ret;
}

void store_iNode(inode *ip); //store an inode to disk
inode *load_iNode(uint inum); // load an inode from disk
#endif
