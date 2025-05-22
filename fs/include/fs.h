#ifndef __FS_H__
#define __FS_H__

#include "block.h"
#include "common.h"
#include "inode.h"
#include <stddef.h>

// used for cmd_ls
typedef struct { //type = directory
    char name[MAXNAME];
    uint type;
    uint inum;
    ushort owner;
    ushort permission;
    uint modTime;
} entry;


void sbinit();

int cmd_f();

int cmd_mk(char *name, short mode);
int cmd_mkdir(char *name, short mode);
int cmd_rm(char *name);
int cmd_rmdir(char *name);

int cmd_cd(char *name);
int cmd_ls(entry **entries, int *n);

int cmd_cat(char *name, uchar **buf, uint *len);
int cmd_w(char *name, uint len, const char *data);
int cmd_i(char *name, uint pos, uint len, const char *data);
int cmd_d(char *name, uint pos, uint len);

int cmd_login(int auid);
bool is_formated();
int user_init(uint uid);
int user_end(uint uid);
void cmd_exit(uint u);

int cd_to_home(int auid);
int cmd_pwd(char **buf, size_t buflen);
#endif