#include "../include/fs.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include "../include/block.h"
#include "../../include/log.h"
#include "../include/common.h"
#include "../include/inode.h"

entry curDir;
void sbinit() {
    
}

int cmd_f(int ncyl, int nsec) {
 /* Format. This will format the file system on the disk, by initializing any/all of the tables that
 the file system relies on*/
    sb.size = ncyl * nsec;
    _mount_disk();
    inode *root ;
    if(sb.root == 0){
        Warn("cmd_f: file system uninitialized");
        root = ialloc(T_DIR);
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

    strcpy(curDir.name, root->name);
    curDir.type = root->type;
    curDir.inum = root->inum;
    curDir.owner = root->owner;
    curDir.permission = root->permission;

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
    //mkdir d: Create a directory. This will create a subdirectory named d in the current directory

    if(_check_duiplicate(name)){
        Error("cmd_mk: file %s already exists", name);
        return E_ERROR;
    }


    inode *subdir = ialloc(T_DIR);
    if(subdir == NULL){
        Error("cmd_mkdir: subdir allocation failed");
        return E_ERROR;
    }
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
    uint *links = (uint*)malloc(dir->fileSize);
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
    uint *links = (uint*)malloc(total_bytes);
    if (!links) {
        Error("cmd_ls: malloc links failed");
        return E_ERROR;
    }
    readi(dir, (uchar*)links, 0, total_bytes);
    uint total = total_bytes / sizeof(uint);
    *n = total > 2 ? total - 2 : 0;
    *entries = (entry *)malloc((*n) * sizeof(entry));
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
            uint *links = (uint* )malloc(total_bytes);
            readi(ptr, (uchar*)links, 0, total_bytes);
            iput(ptr);
            ptr = iget(links[1]);
            free(links);
            if (!ptr) {
                Error("cmd_cd: parent directory cannot be found");
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
            Error("cmd_cd: directory %s not found", tok);
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
    inode *dst = _path_finder(name);
    if (!dst) {
        Error("cmd_cd: path %s not found", name);
        return E_ERROR;
    }
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

    uint *links =(uint* ) malloc(dir->fileSize);
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
    
    inode *cur = iget(curDir.inum);
    uint *links = (uint* )malloc(cur->fileSize);
    readi(cur, (uchar *)links, 0, cur->fileSize);
    uint total = cur->fileSize / sizeof(uint);
    uint targetPos = 0;
    for(int i=2;i<total;i++){
        if(links[i] == target->inum){
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

    uint *links = (uint *)malloc(dir->fileSize);
    readi(dir, (uchar *)links, 0, dir->fileSize);
    uint total = dir->fileSize / sizeof(uint);

    *n = total - 2; // exclude "." and ".."
    // allocate array for entries
    *entries = (entry *)malloc((*n) * sizeof(entry));
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

    if(!ip){
        Error("cmd_w: file %s not found", name);
        return E_ERROR;
    }

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

int cmd_exit(){
    if(sb.bitmap != NULL){
        free(sb.bitmap);
        sb.bitmap = NULL;
        return E_SUCCESS;
    }
    fprintf(stderr, "cmd_exit: sb.bitmap cannot be NULL\n");
    return E_ERROR;
}

int cmd_login(int auid) {
    return E_SUCCESS;
}

