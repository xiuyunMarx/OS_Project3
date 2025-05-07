#include "../include/disk.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "../../include/log.h"

// global variables
int _ncyl, _nsec, ttd;
char *diskFile;
int fd;
int FILE_SIZE = 0; //n bytes
int lastCyl = 0; // last cylinder accessed
int init_disk(char *filename, int ncyl, int nsec, int ttd) {
    _ncyl = ncyl;
    _nsec = nsec;
    ttd = ttd;
    lastCyl = 0;
    FILE_SIZE = ncyl * nsec * BLOCKSIZE;
    int fd = open(filename , O_RDWR | O_CREAT, 0644);
    if(fd == -1){
        Log("Error opening file");
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

    diskFile = mmap(NULL,FILE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
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
int cmd_i(int *ncyl, int *nsec) {
    // get the disk info
    *ncyl = _ncyl;
    *nsec = _nsec;
    return 0;
}

int cmd_r(int cyl, int sec, char *buf) {
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
    if(msync(diskFile, start * BLOCKSIZE, MS_SYNC) == -1){
        Log("error when syncing file to disk\n");
        return -1;
    }// sync the file to disk

    diskDelay(lastCyl, cyl); // simulate the delay between cylinders
    lastCyl = cyl; // update the last cylinder accessed

    return 0;
}

int cmd_w(int cyl, int sec, int len, char *data) {
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
        }
    }

    int start = cyl * _nsec + sec;
    memcpy(diskFile + start*BLOCKSIZE, buf, BLOCKSIZE);
    if(msync(diskFile, start * BLOCKSIZE, MS_SYNC) == -1){
        Log("error when syncing file to disk\n");
        free(buf);
        return -1;
    }// sync the file to disk
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

void diskDelay(int c1, int c2){ // simulating the delay between cylinders
    int delay = ttd * abs(c1 - c2);
    usleep(delay);
    if(delay > 0){
        fprintf(stderr, "Disk delay: %d microseconds\n", delay);        
    }

}