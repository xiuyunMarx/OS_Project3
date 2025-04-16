#ifndef __DISK_H__
#define __DISK_H__
#include <string.h>
#define BLOCKSIZE 512

int init_disk(char* filename, int ncyl, int nsec, int ttd);
int cmd_i(int *ncyl, int *nsec);
int cmd_r(int cyl, int sec, char *buf);
int cmd_w(int cyl, int sec, int len, char *data);
void close_disk();
void diskDelay(int c1, int c2);

#endif
