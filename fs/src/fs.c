#include "../include/fs.h"

#include <asm-generic/errno.h>
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include "../include/block.h"
#include "../../include/log.h"
#include "../include/common.h"
#include "../include/inode.h"

entry curDir, backUp;
uint curUser=1;

void sbinit() {
    curUser  = 1;
}

bool is_formated() {
    if (sb.root == 0 || sb.size == 0) {
        return false;
    }
    return true;
}

entry _fetch_entry(uint inum){
    inode *dir = iget(inum);
    entry ret;
    ret.inum = 0;
    if(dir == NULL){
        Error("_fetch_entry: fetching inode failed");
        return ret;
    }
    ret.inum = inum;
    ret.type = dir->type;
    assert(ret.type == T_DIR);
    ret.owner = dir->owner;
    ret.permission = dir->permission;
    ret.modTime = dir->modTime;
    strcpy(ret.name, dir->name);
    iput(dir);
    return ret;
}

int user_init(uint uid){
    for(int i=0;i<MAXUSERS;i++){
        if(sb.users[i].uid == uid){
            backUp = curDir;
            if(sb.users[i].cwd == 0){
                sb.users[i].cwd = sb.root;
                Warn("user %d : no cwd, set to HOME", uid);
                cd_to_home(uid);
            }
            curDir = _fetch_entry(sb.users[i].cwd);
            curUser = uid;
            Log("User %d : Found, cwd = %s ", uid, curDir.name);
            return E_SUCCESS;
        }
    }
    Error("user %d : please login first", uid);
    return E_ERROR;
}

enum {
    READ = 4,
    WRITE = 2,
    EXECUTE = 1,
};
uint _permission_check(inode *ip){
    if(curUser == 1){
        return READ | WRITE | EXECUTE; //root can access everything
    }
    if(ip == NULL){
        fprintf(stderr, "_permission check: Error: ip cannot be NULL, you dump developer\n");
        Error("_permission check: Error: ip cannot be NULL, you dump developer");
        return E_ERROR;
    }
    if(curUser == ip->owner){
        ushort p = ip->permission & 0b111000; //the first 3 bits
        switch (p) {
            case 0b111000: return READ | WRITE | EXECUTE;
            case 0b110000: return READ | WRITE;
            case 0b101000: return READ | EXECUTE;
            case 0b100000: return READ;
            case 0b011000: return WRITE | EXECUTE;
            case 0b010000: return WRITE;
            case 0b001000: return EXECUTE;
            default: return 0;
        }
    }else{
        ushort p = ip->permission & 0b000111; //the last 3 bits
        switch (p) {
            case 0b000111: return READ | WRITE | EXECUTE;
            case 0b000110: return READ | WRITE;
            case 0b000101: return READ | EXECUTE;
            case 0b000100: return READ;
            case 0b000011: return WRITE | EXECUTE;
            case 0b000010: return WRITE;
            case 0b000001: return EXECUTE;
            default: return 0;
        }
    }
}

int user_end(uint uid){
    //update the status when a user finishes executing a command
    curUser = 1;
    for(int i=0;i<MAXUSERS;i++){
        if(sb.users[i].uid == uid){
            sb.users[i].cwd = curDir.inum;
            curDir = backUp;
            return E_SUCCESS;
        }
    }
    Error("user %d : Not Found", uid);
    return E_ERROR;
}

int cmd_f() {
 /* Format. This will format the file system on the disk, by initializing any/all of the tables that
 the file system relies on*/
    if(curUser != 1){
        Error("cmd_f: permisssion denied, only ROOT can format the disk");
        return E_ERROR;
    }
    superblock tmp_sb;
    memcpy(&tmp_sb, &sb, sizeof(superblock)); //backup user information

    _mount_disk();
    inode *root ;
    if(sb.root == 0){
        Warn("cmd_f: file system uninitialized");
        root = ialloc(T_DIR);
        root->owner = 1145; //root can be accessed by any user
        root->permission = 0b111111; //root can be accessed by any user
        if(root == NULL){
            Error("cmd_f: root allocation failed");
            return E_ERROR;
        }
        strcpy(root->name, "/");
        uint hardlink[2] = {root->inum, root->inum};
        writei(root, (uchar *)hardlink, 0, sizeof(hardlink));
         //add hardlink to root . and ..
        sb.root = root->inum;
    }else{
        root = iget(sb.root);
        if(root == NULL){
            Error("cmd_f: root inode error");
            return E_ERROR;
        }
    }
    memcpy(&sb.users[0], &tmp_sb.users[0], sizeof(tmp_sb.users)); //restore user information

    strcpy(curDir.name, root->name);
    curDir.type = root->type;
    curDir.inum = root->inum;
    curDir.owner = root->owner;
    curDir.permission = root->permission;

    sbinit();
    iput(root);
    return E_SUCCESS;
}

bool _check_duiplicate(char *name) {
    //check if the name is already in the current directory
    entry *entries;
    int n;
    cmd_ls(&entries, &n);
    for(int i = 0; i < n; i++){
        if(strcmp(entries[i].name, name) == 0){
            free(entries);
            return true;
        }
    }
    free(entries);
    return false;
}

int cmd_mk(char *name, short mode) {
    //mk f: Create a file. This will create a file named f in the file system.
    //mode is the permission of the file
    inode *tmp = iget(curDir.inum);
    if(tmp == NULL){
        Error("cmd_mk: dir cannot be found");
        return E_ERROR;
    }
    ushort res = _permission_check(tmp);
    // require write+execute on directory
    if((res & (WRITE|EXECUTE)) != (WRITE|EXECUTE)){
        Error("cmd_mk: permission denied");
        iput(tmp);
        return E_ERROR;
    }
    iput(tmp);
    /*permission check on creating file*/

    if(_check_duiplicate(name)){
        Error("cmd_mk: file %s already exists", name);
        return E_ERROR;
    }

    inode *file = ialloc(T_FILE);
    if(file == NULL){
        Error("cmd_mk: file allocation failed");
        return E_ERROR;
    }
    strcpy(file->name , name);
    file->permission = mode;
    file->owner = curUser;
    file->modTime = time(NULL); //create a new File, and write in owner

    inode *dir = iget(curDir.inum);
    if(dir == NULL){
        Error("cmd_mk: dir cannot be found");
        iput(file);
        return E_ERROR;
    }
    writei(dir,(uchar *)&(file->inum), dir->fileSize, sizeof(file->inum));
    dir->modTime = file->modTime = time(NULL);
    dir->linkCount++;
    iupdate(dir);
    iupdate(file);
    iput(dir);
    iput(file);
    return E_SUCCESS;
}

int cmd_mkdir(char *name, short mode) {
    //mkdir d: Create a directory. This will create a subdirectory named d in the current directory, curDir NOT CHANGE
    if(_check_duiplicate(name)){
        Error("cmd_mk: file %s already exists", name);
        return E_ERROR;
    }

    inode *tmp = iget(curDir.inum);
    if(tmp == NULL){
        Error("cmd_mkdir: current directory Error, cannot be found");
        return E_ERROR;
    }
    ushort res = _permission_check(tmp);
    // require write+execute on directory
    if((res & (WRITE|EXECUTE)) != (WRITE|EXECUTE)){
        Error("cmd_mkdir: permission denied");
        iput(tmp);
        return E_ERROR;
    }
    iput(tmp);
    /*permission check on creating sub directory*/

    inode *subdir = ialloc(T_DIR);
    if(subdir == NULL){
        Error("cmd_mkdir: subdir allocation failed");
        return E_ERROR;
    }
    subdir->owner = curUser;
    subdir->modTime = time(NULL);
    subdir->permission = mode;
    strcpy(subdir->name,name);
    
    inode *cur = iget(curDir.inum);
    if(cur == NULL){
        Error("cmd_mkdir: curDir cannot be found");
        iput(subdir);
        return E_ERROR;
    }
    uint hardlinks[] = {subdir->inum, cur->inum};
    writei(subdir, (uchar *)hardlinks, 0, sizeof(uint) * 2); //add hardlink to subdir . and ..
    writei(cur, (uchar *)&(subdir->inum),cur->fileSize, sizeof(subdir->inum)); //add link of new sub dir to curDir

    subdir->modTime = cur->modTime = time(NULL);
    subdir->linkCount = 2;
    cur->linkCount++;

    iupdate(cur);
    iupdate(subdir);

    iput(cur);
    iput(subdir);
    return E_SUCCESS;
}

int cmd_rm(char *name) {
    // rm f: Delete file. This will delete the file named f from the current directory.
    inode *dir = iget(curDir.inum);
    if(dir == NULL){
        Error("cmd_rm: dir cannot be found");
        return E_ERROR;
    }
    ushort dir_perm = _permission_check(dir);
    if(!(dir_perm & WRITE)){
        Error("cmd_rm: permission denied");
        iput(dir);
        return E_ERROR;
    } /*permission check on current directory*/

    uint *links =(uint*) malloc(dir->fileSize);
    readi(dir, (uchar *)links, 0, dir->fileSize);
    uint total = dir->fileSize / sizeof(uint);

    uint targetPos = 0;
    bool found = false;
    for(int i = 2;i<total;i++){ //skip . and ..
        inode *sub = iget(links[i]);
        if(sub == NULL){
            Error("cmd_rm: sub file cannot be found");
            free(links);
            return E_ERROR;
        }
        if(sub->type == T_FILE && strcmp(sub->name, name) == 0){
            //found the file to delete
            ushort file_perm = _permission_check(sub);
            if(!(file_perm & WRITE)){
                Error("cmd_rm: permission denied");
                iput(sub);
                free(links);
                iput(dir);
                return E_ERROR;
            } /*permission check on deleting file*/

            //remove the file
            dir->linkCount--;
            links[i] = 0;
            targetPos = i;
            wipeout_inode(sub); //remove the inode from disk;
            iput(sub);
            found = true;
            break;
        }
        iput(sub);
    }
    if(!found){
        Error("rm: file %s not found", name);
        free(links);
        iput(dir);
        return E_ERROR;
    }

    //shift the links array to remove the deleted file
    for(int i = targetPos;i<total-1;i++){
        links[i] = links[i+1];
    }
    links[total-1] = 0; //set the last link to 0
    writei(dir, (uchar *)links, 0, dir->fileSize);    
    dir->fileSize-=sizeof(uint);
    //don't change block size, for we have to keep consistency when wiping out the inode
    //the allocated data blocks for an inode won't decrease
    dir->modTime = time(NULL);
    iupdate(dir);

    free(links);
    iput(dir);
    return E_SUCCESS;
}

int _ls_entries(inode *dir, entry **entries, int *n) {
    if (!dir) {
        Error("cmd_ls: dir cannot be found");
        return E_ERROR;
    }
    uint total_bytes = dir->fileSize;
    uint *links = (uint *)malloc(total_bytes);
    if (!links) {
        Error("cmd_ls: malloc links failed");
        return E_ERROR;
    }
    readi(dir, (uchar*)links, 0, total_bytes);
    uint total = total_bytes / sizeof(uint);
    *n = total > 2 ? total - 2 : 0;
    *entries = (entry*)malloc((*n) * sizeof(entry));
    if (!*entries) {
        Error("cmd_ls: malloc entries failed");
        free(links);
        return E_ERROR;
    }
    for (uint i = 2; i < total; i++) {
        inode *sub = iget(links[i]);
        if (!sub) {
            Error("cmd_ls: sub file cannot be found");
            // 清理所有已分配
            for (uint j = 2; j < i; j++) {
                // nothing for now, 因为 name 存在数组里
            }
            free(*entries);
            free(links);
            return E_ERROR;
        }
        int idx = i - 2;
        (*entries)[idx].inum = sub->inum;
        (*entries)[idx].type  = sub->type;
        (*entries)[idx].owner = sub->owner;
        (*entries)[idx].permission = sub->permission;
        (*entries)[idx].modTime    = sub->modTime;
        strncpy((*entries)[idx].name, sub->name, MAXNAME-1);
        (*entries)[idx].name[MAXNAME-1] = '\0';
        iput(sub);
    }
    free(links);
    return E_SUCCESS;
}

inode *_path_finder(const char *name) {
    if (!name[0] || strcmp(name, ".") == 0) {
        return iget(curDir.inum);
    }
    char *path = strdup(name);
    inode *ptr = (path[0]=='/') ? iget(sb.root) : iget(curDir.inum);
    if (!ptr) { free(path); return NULL; }
    if (strcmp(path, "/") == 0) {
        free(path);
        return ptr;
    }
    for (char *tok = strtok(path, "/"); tok; tok = strtok(NULL, "/")) {
        if (!tok[0] || strcmp(tok, ".")==0) continue;
        if (strcmp(tok, "..")==0) {
            // 统一用 fileSize 方式读整个目录块，然后取 links[1]
            uint total_bytes = ptr->fileSize;
            uint *links = (uint*)malloc(total_bytes);
            readi(ptr, (uchar*)links, 0, total_bytes);
            iput(ptr);
            ptr = iget(links[1]);
            free(links);
            if (!ptr) {
                Error("cmd_cd - path finder: parent directory cannot be found");
                free(path);
                return NULL;
            }
            continue;
        }
        entry *entries; int n;
        if (_ls_entries(ptr, &entries, &n) != E_SUCCESS) {
            iput(ptr);
            free(path);
            return NULL;
        }
        bool found = false;
        for (int i = 0; i < n; i++) {
            if (entries[i].type == T_DIR && strcmp(entries[i].name, tok)==0) {
                found = true;
                iput(ptr);
                ptr = iget(entries[i].inum);
                break;
            }
        }
        free(entries);
        if (!found) {
            Error("cmd_cd - path finder: directory %s not found", tok);
            iput(ptr);
            free(path);
            return NULL;
        }
        if (!ptr) {
            free(path);
            return NULL;
        }
    }
    free(path);
    return ptr;
}

int cmd_cd(char *name) {
    /*this function will change the curDir to the 'name ' */
    inode *dst = _path_finder(name);
    if (!dst) {
        Error("cmd_cd: path %s not found", name);
        return E_ERROR;
    }

    ushort res = _permission_check(dst);
    if(!(res & EXECUTE)){
        Error("cmd_cd: permission denied, cannot access to %s", name);
        iput(dst);
        return E_ERROR;
    } /*permission check on changing directory*/

    curDir.inum = dst->inum;
    curDir.type = dst->type;
    assert(curDir.type == T_DIR);
    curDir.owner = dst->owner;
    curDir.permission = dst->permission;
    strcpy(curDir.name, dst->name);
    curDir.modTime = dst->modTime;

    iput(dst);
    return E_SUCCESS;
}

bool _has_file(inode *dir){
    //check if the directory has any files recursively
    if(dir == NULL){
        Error("cmd_rmdir: dir cannot be NULL");
        return false;
    }

    uint *links = (uint*)malloc(dir->fileSize);
    readi(dir, (uchar *)links, 0, dir->fileSize);
    uint total = dir->fileSize / sizeof(uint);
    bool hasFile = false; 
    for(int i=2;i<total;i++){
        inode *sub = iget(links[i]);
        if(sub->type == T_FILE){
            hasFile = true;
            iput(sub);
            break;
        } else if(sub->type == T_DIR){
            hasFile = _has_file(sub);
            iput(sub);
            if(hasFile){
                break;
            }
        }else{
            iput(sub);          
        }

    }
    free(links);
    return hasFile;
}

int _rmdir_helper(inode *dir){
    //delete directories recursively
    if(dir == NULL){
        Error("cmd_rmdir: dir cannot be found");
        return E_ERROR;
    }
    uint *links = (uint*)malloc(dir->fileSize);
    readi(dir, (uchar *)links, 0, dir->fileSize);
    uint total = dir->fileSize / sizeof(uint);

    for(int i=2;i<total;i++){
        inode *sub = iget(links[i]);
        if(sub == NULL){
            Error("cmd_rmdir: sub file cannot be found");
            free(links);
            return E_ERROR;
        }
        if(sub->type == T_DIR){
            _rmdir_helper(sub);
        }else{
            //delete file
            wipeout_inode(sub); //remove the inode from disk
            iput(sub);
        }
    } 
    //这里不再需要更新dir 的链接, 因为本directory即将被删除
    free(links);
    wipeout_inode(dir); //remove the directory inode from disk
    iput(dir);//wipeout_inode 结尾没有iput
    //delete the directory itself
    return E_SUCCESS;
}

int cmd_rmdir(char *name) {
    // rmdir d: Delete a directory. This will delete the subdirectory named d within the current
    // directory
    inode *target = _path_finder(name);
    if(!target) {
        Error("cmd_rmdir: path %s not found", name);
        return E_ERROR;
    }
    if(target->type != T_DIR){
        Error("cmd_rmdir: %s is not a directory", name);
        iput(target);
        return E_ERROR;
    }
    if(_has_file(target)){
        Error("cmd_rmdir: directory %s is not empty", name);
        iput(target);
        return E_ERROR;
    }
    
    inode *cur = iget(curDir.inum); //获取当前所在的目录
    ushort curDir_perm = _permission_check(cur);
    if(!(curDir_perm & WRITE)){
        Error("cmd_rmdir: Cannot remove diretory under current directory %s", cur->name);
        iput(cur);
        iput(target);
        return E_ERROR;
    } /*permission check on current directory*/

    
    uint *links = (uint*)malloc(cur->fileSize);
    readi(cur, (uchar *)links, 0, cur->fileSize);
    uint total = cur->fileSize / sizeof(uint);
    uint targetPos = 0;
    for(int i=2;i<total;i++){
        if(links[i] == target->inum){
            ushort target_perm = _permission_check(target);
            if(!(target_perm & WRITE)){
                Error("cmd_rmdir: Cannot remove directory %s", name);
                free(links);
                iput(cur);
                iput(target);
                return E_ERROR;
            } /*permission check on deleting directory*/

            //remove the directory
            targetPos = i;
            links[i] = 0;
            break;
        }
    }
    for(int i=targetPos;i<total-1;i++){
        links[i] = links[i+1];
    }
    links[total-1] = 0; //set the last link to 0
    writei(cur, (uchar *)links, 0, cur->fileSize);    
    cur->linkCount--;
    cur->fileSize -= sizeof(uint);
    cur->modTime = time(NULL);

    free(links);
    iput(cur); //remove the link to target dir from its parent
    _rmdir_helper(target); //delete the target dir and its contents
    return E_SUCCESS;
}


int cmd_ls(entry **entries, int *n) {
    // ls: Directory listing. This will return a listing of the files and directories in the current directory.
    // You are also required to return other meta information, such as file size, last update time, etc
    // n is the number of entries, it won't include the "." and ".." entries
    inode *dir = iget(curDir.inum);
    if(dir == NULL){
        Error("cmd_ls: dir cannot be found");
        return E_ERROR;
    }
    ushort dir_perm = _permission_check(dir);
    if(!(dir_perm & READ)){
        Error("cmd_ls: permission denied");
        iput(dir);
        return E_ERROR;
    } /*permission check on current directory*/

    assert(dir->type == T_DIR);
    uint *links = (uint *)malloc(dir->fileSize);
    readi(dir, (uchar *)links, 0, dir->fileSize);
    uint total = dir->fileSize / sizeof(uint);

    *n = total - 2; // exclude "." and ".."
    // allocate array for entries
    *entries = (entry*)malloc((*n) * sizeof(entry));
    if (*entries == NULL) {
        Error("cmd_ls: malloc entries failed");
        free(links);
        iput(dir);
        return E_ERROR;
    }

    for(uint i = 2; i < total; i++) {
        inode *sub = iget(links[i]);
        if(sub == NULL){
            Error("cmd_ls: sub file cannot be found");
            free(links);
            return E_ERROR;
        }

        int idx = i - 2;
        (*entries)[idx].inum = sub->inum;
        (*entries)[idx].type = sub->type;
        (*entries)[idx].owner = sub->owner;
        (*entries)[idx].permission = sub->permission;
        (*entries)[idx].modTime = sub->modTime;
        memcpy((*entries)[idx].name, sub->name, MAXNAME);

        iput(sub);
    }

    free(links);
    iput(dir);
    return E_SUCCESS;
}

int cmd_cat(char *name, uchar **buf, uint *len) {
    inode *cur = iget(curDir.inum);
    if(cur == NULL){
        Error("cmd_cat: curDir cannot be found");
        return E_ERROR;
    }
    uint *links = (uint *)malloc(cur->fileSize);
    readi(cur, (uchar*)links, 0, cur->fileSize);
    uint total = cur->fileSize / sizeof(uint);
    iput(cur);

    inode *ip = NULL;
    for(int i=2;i<total;i++){
        inode *sub = iget(links[i]);
        if(sub == NULL){
            Error("cmd_cat: file cannot be NULL");
            free(links);
            return E_ERROR;
        }
        if(sub->type == T_FILE && strcmp(sub->name, name) == 0){
            ip = sub;
            break;
        }
        iput(sub);
    }
    free(links);
    if(ip == NULL){
        Error("cmd_w: file %s not found", name);
        return E_ERROR;
    }  //after above , ip the file inode
    ushort file_perm = _permission_check(ip);
    if(!(file_perm & READ)){
        Error("cmd_cat: permission denied");
        iput(ip);
        return E_ERROR;
    } /*permission check on reading file*/

    assert(ip->type == T_FILE);
    if (!ip) {
        Error("cmd_cat: file %s not found", name);
        return E_ERROR;
    }
    if (ip->type != T_FILE) {
        iput(ip);
        Error("cmd_cat: %s is not a file", name);
        return E_ERROR;
    }
    *len = ip->fileSize;
    *buf = (uchar *)malloc(*len);
    if (*buf == NULL) {
        iput(ip);
        Error("cmd_cat: malloc failed");
        return E_ERROR;
    }
    if (readi(ip, *buf, 0, *len) < 0) {
        free(*buf);
        iput(ip);
        Error("cmd_cat: read failed");
        return E_ERROR;
    }
    iput(ip);
    return E_SUCCESS;
}

int cmd_w(char *name, uint len, const char *data) {
/*w f l data: Write data into file. This will overwrite the contents of the file named f with the
 l bytes of data. If the new data is longer than the data previously in the file, the file will be
 extended longer. If the new data is shorter than the data previously in the file, the file will be
 truncated to the new length*/
    //write to a file under the current directory
    inode *cur = iget(curDir.inum);
    if(cur == NULL){
        Error("cmd_w: curDir cannot be found");
        return E_ERROR;
    }
    uint *links = (uint *)malloc(cur->fileSize);
    readi(cur, (uchar*)links, 0, cur->fileSize);
    uint total = cur->fileSize / sizeof(uint);
    iput(cur);

    inode *ip = NULL;
    for(int i=2;i<total;i++){
        inode *sub = iget(links[i]);
        if(sub == NULL){
            Error("cmd_w: file cannot be NULL");
            free(links);
            return E_ERROR;
        }
        if(sub->type == T_FILE && strcmp(sub->name, name) == 0){
            ip = sub;
            break;
        }
        iput(sub);
    }
    free(links);
    if(ip == NULL){
        Error("cmd_w: file %s not found", name);
        return E_ERROR;
    }

    ushort file_perm = _permission_check(ip);
    if(!(file_perm & WRITE)){
        Error("cmd_w: permission denied");
        iput(ip);
        return E_ERROR;
    } /*permission check on writing file*/

    assert(ip->type == T_FILE);
    if(len < ip->fileSize){
        //truncate the file;
        uchar *buf = (uchar *)malloc(ip->fileSize);
        memset(buf, 0 , ip->fileSize);
        memcpy(buf, data, len);
        int res = writei(ip, buf, 0, ip->fileSize); 
        //the trailing part will be zeroed
        if(res < 0){
            Error("cmd_w: write failed when truncating");
            free(buf);
            iput(ip);
            return E_ERROR;
        }
        ip->fileSize = len;
        ip->modTime = time(NULL);
        iupdate(ip);
        free(buf);
    }else{
        writei(ip, (uchar *)data, 0, len);
        ip->fileSize = len;
        ip->modTime = time(NULL);
        iupdate(ip);
        //update the file size
    }
    iput(ip);
    return E_SUCCESS;
}

int cmd_i(char *name, uint pos, uint len, const char *data) {
    /* Insert data to a file. This will insert l bytes of data into the file after the pos−1th
    character but before the posth character (0-indexed). If the pos is larger than the size of the file,
    append the l bytes of data to the end of the file.*/
    inode *cur = iget(curDir.inum);
    if(cur == NULL){
        Error("cmd_i: curDir cannot be found");
        return E_ERROR;
    }
    uint *links = (uint *)malloc(cur->fileSize);
    readi(cur, (uchar*)links, 0, cur->fileSize);
    uint total = cur->fileSize / sizeof(uint);
    iput(cur);
    inode *ip = NULL;
    for(int i=2;i<total;i++){
        inode *sub = iget(links[i]);
        if(sub == NULL){
            Error("cmd_i: file cannot be NULL");
            free(links);
            return E_ERROR;
        }
        if(sub->type == T_FILE && strcmp(sub->name, name) == 0){
            ip = sub;
            break;
        }
        iput(sub);
    }
    free(links);
    if(ip == NULL){
        Error("cmd_i: file %s not found", name);
        return E_ERROR;
    }
    //after above , ip the file inode

    ushort file_perm = _permission_check(ip);
    if(!(file_perm & WRITE)){
        Error("cmd_i: permission denied");
        iput(ip);
        return E_ERROR;
    } /*permission check on writing file*/

    assert(ip->type == T_FILE);
    if(pos > ip->fileSize){
        uchar *buf = (uchar *)malloc(ip->fileSize + len);
        memset(buf, 0 , ip->fileSize + len);
        readi(ip, buf, 0, ip->fileSize);
        memcpy(buf + ip->fileSize, data , len);
        writei(ip, buf, 0, ip->fileSize + len); 
        //we don't increase the file size, since it has been updated in writei
        ip->modTime = time(NULL);
        iupdate(ip);
        free(buf); //append the data to the end of the file
    }else{
        assert(pos <= ip->fileSize);
        uchar *buf = (uchar *)malloc(ip->fileSize + len);
        memset(buf, 0 , ip->fileSize + len);
        readi(ip , buf, 0, ip->fileSize);
        memcpy(buf + pos + len, buf + pos, ip->fileSize - pos);
        memcpy(buf + pos, data, len);
        writei(ip, buf, 0, ip->fileSize + len);
        ip->modTime = time(NULL);
        iupdate(ip);
        free(buf); //insert the data to the file
        //we don't increase the file size, since it has been updated in writei
    }

    iput(ip);
    return E_SUCCESS;
}

int cmd_d(char *name, uint pos, uint len) {
    /* d f pos l: Delete data in the file. This will delete l bytes starting from the pos character
 (0-indexed), or till the end of the file (if l is larger than the remaining length of the file)*/
    inode *cur = iget(curDir.inum);
    if(cur == NULL){
        Error("cmd_d: curDir cannot be found");
        return E_ERROR;
    }
    uint *links = (uint *)malloc(cur->fileSize);
    readi(cur, (uchar*)links, 0, cur->fileSize);
    uint total = cur->fileSize / sizeof(uint);
    iput(cur);
    inode *ip = NULL;
    for(int i=2;i<total;i++){
        inode *sub = iget(links[i]);
        if(sub == NULL){
            Error("cmd_d: file cannot be NULL");
            free(links);
            return E_ERROR;
        }
        if(sub->type == T_FILE && strcmp(sub->name, name) == 0){
            ip = sub;
            break;
        }
        iput(sub);
    }
    free(links);
    if(ip == NULL){
        Error("cmd_d: file %s not found", name);
        return E_ERROR;
    }
    //after above , ip the file inode

    ushort file_perm = _permission_check(ip);
    if(!(file_perm & WRITE)){
        Error("cmd_d: permission denied");
        iput(ip);
        return E_ERROR;
    } /*permission check on writing file*/

    assert(ip->type == T_FILE);
    if(pos >= ip->fileSize){
        Error("cmd_d: pos %d is larger than file size %d", pos, ip->fileSize);
        iput(ip);
        return E_ERROR;
    }else{
        uchar *buf = (uchar *)malloc(ip->fileSize);
        readi(ip, buf, 0, ip->fileSize);
        if(pos + len > ip->fileSize){
            //zero all data after pos
            memset(buf + pos, 0, ip->fileSize - pos);
            writei(ip, buf, 0, ip->fileSize);
            ip->fileSize = pos;
            ip->modTime = time(NULL);
            iupdate(ip);
            free(buf);
        }else{
            //shift the data after pos
            memcpy(buf + pos, buf + pos + len, ip->fileSize - pos - len);
            writei(ip, buf, 0, ip->fileSize - len);
            ip->modTime = time(NULL);
            ip->fileSize -= len;
            iupdate(ip);
            free(buf);
            //update the file size}
        }    
    }
    iput(ip);
    return E_SUCCESS;
}

int cmd_login(int auid) {
    for(int i=0;i<MAXUSERS;i++){
        if(sb.users[i].uid == auid){
            fprintf(stderr,"User %d already logged in\n", auid);
            return E_ERROR;
        }
        if(sb.users[i].uid == 0){
            sb.users[i].uid = auid;
            sb.users[i].cwd = sb.root;
            fprintf(stderr,"User %d logged in\n", auid);
            return E_SUCCESS;
        }
    } 
    return E_SUCCESS;
}

void cmd_exit(uint u){
    uchar *buf = (uchar *)malloc(BSIZE);
    memset(buf, 0, BSIZE);
    memcpy(buf, &sb, sizeof(superblock));
    write_block(0, buf);
    free(buf);


    exit_block();
    for(int i=0;i<MAXUSERS;i++){
        if(sb.users[i].uid == u){
            sb.users[i].uid = 0;
            fprintf(stderr,"User %d logged out\n", u);
            break;
        }
    }
}

int cd_to_home(int auid){
    backUp = curDir;
    curUser = 1; //set to root
    curDir = _fetch_entry(sb.root);
    int res = cmd_cd("/home");
    if(res == E_ERROR){
        Warn("Home not found, initializing home");
        curDir = _fetch_entry(sb.root);
        if(cmd_mkdir("home", 0b111111) == E_ERROR){
            Error("Home directory creation failed");
            return E_ERROR;
        }
        if(cmd_cd("home") == E_ERROR){
            Error("Error changing to Home directory");
            return E_ERROR;
        }
    }
    //change the current directory to home, if Not found, create it

    char user_home[MAXNAME];
    sprintf(user_home, "%d", auid);
    if(cmd_cd(user_home) == E_ERROR){
        if(cmd_mkdir(user_home, 0b111111) == E_ERROR){
            Error("User home directory creation failed");
            return E_ERROR;
        }
        if(cmd_cd(user_home) == E_ERROR){
            Error("Error changing to User home directory");
            return E_ERROR;
        }
    }
    //create the user home directory
    assert(curDir.type == T_DIR && strcmp(curDir.name, user_home) == 0);

    for(int i=0;i<MAXUSERS;i++){
        if(sb.users[i].uid == auid){
            sb.users[i].cwd = curDir.inum;
            Log("User %d home: /home/%s", auid, curDir.name);
            break;
        } 
    }
    curDir = backUp;
    curUser = auid; //set back to user
    return E_SUCCESS;
}

int cmd_pwd(char **out, size_t buflen){
    *out = (char *)malloc(BSIZE);
    *out[0] = '\0';
    inode *d = iget(curDir.inum);
    if(d == NULL){
        Error("cmd_pwd: current directory cannot be found");
        return E_ERROR;
    }
    if(d->inum == sb.root){
        strcpy(*out, "/");
        iput(d);
        return E_SUCCESS;
    }else{

        while(d->inum != sb.root){
            assert(d->type == T_DIR);

            char *tmp = (char *)malloc(MAXNAME);
            strcpy(tmp, d->name);
            char *ret = (char *)malloc(buflen);
            memset(ret, 0, buflen);

            sprintf(ret, "/%s%s", tmp, *out);
            strcpy(*out, ret);
            free(tmp);
            free(ret);
            
            uint *links = (uint *)malloc(d->fileSize);
            readi(d, (uchar *)links, 0, d->fileSize);
            uint parent = links[1];
            iput(d);
            d = iget(parent);
        }        
    }

    return E_SUCCESS;
}