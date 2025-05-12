#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../include/block.h"
#include "../include/common.h"
#include "../include/fs.h"
#include "../../include/log.h"

// global variables
int ncyl, nsec;

#define ReplyYes()       \
    do {                 \
        printf("Yes\n"); \
        Log("Success");  \
    } while (0)
#define ReplyNo(x)      \
    do {                \
        printf("No\n"); \
        Warn(x);        \
    } while (0)

// return a negative value to exit
int handle_f(char *args) {
    char *tk = strtok(args, " ");
    ncyl = atoi(tk);
    tk = strtok(NULL, " ");
    nsec = atoi(tk);
    if (ncyl <= 0 || nsec <= 0) {
        ReplyNo("Invalid arguments");
        return 1;
    }
    if (cmd_f(ncyl, nsec) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to format");
    }
    return 0;
}

int handle_mk(char *args) {
    char *name;
    short mode = 0;

    name = malloc(MAXNAME);
    char *tk = strtok(args, " ");
    strcpy(name, tk);
    tk = strtok(NULL, " ");
    if(tk != NULL){
        mode = atoi(tk);
    }else{
        mode = 0777;
    }
    if (cmd_mk(name, mode) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to create file");
    }

    free(name);
    return 0;
}

int handle_mkdir(char *args) {
    char *name;
    short mode = 0;
    name = malloc(MAXNAME);
    char *tk = strtok(args, " ");
    strcpy(name, tk);
    tk = strtok(NULL, " ");
    if (tk != NULL) {
        mode = atoi(tk);
    }

    if (mode < 0 || mode > 0777) {
        ReplyNo("Invalid mode");
        free(name);
        return 1;
    }

    if (cmd_mkdir(name, mode) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to create file");
    }
    free(name);
    return 0;
}

int handle_rm(char *args) {
    char *name;
    name = malloc(MAXNAME);
    strcpy(name, args);
    if (cmd_rm(name) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to remove file");
    }
    free(name);
    return 0;
}

int handle_cd(char *args) {
    char *name;
    name = malloc(MAXNAME);
    strcpy(name, args);
    if (cmd_cd(name) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to change directory");
    }
    free(name);
    return 0;
}

int handle_rmdir(char *args) {
    char *name;
    name = malloc(MAXNAME);
    strcpy(name, args);
    if (cmd_rmdir(name) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to remove directory");
    }
    free(name);
    return 0;
}

int handle_ls(char *args) {
    entry *entries = NULL;
    int n = 0;
    if (cmd_ls(&entries, &n) != E_SUCCESS) {
        ReplyNo("Failed to list files");
        return 0;
    }
    for(int i=0;i<n;i++){
        printf("%s\t", entries[i].name);
        printf("Type: %s\n", entries[i].type == T_FILE ? "File" : "Directory");
    }
    ReplyYes();
    free(entries);
    return 0;
}

int handle_cat(char *args) {
    char *name = malloc(MAXNAME);
    char *tk = strtok(args, " ");
    if (!tk) { free(name); Error("handle_cat: Invalid arguments"); return 1; }
    strcpy(name, tk);

    uchar *buf = NULL;
    uint len;
    if (cmd_cat(name, &buf, &len) == E_SUCCESS) {
        ReplyYes();
        printf("%.*s\n", len, buf);
        free(buf);
    } else {
        ReplyNo("Failed to read file");
    }
    free(name);
    return 0;
}

int handle_w(char *args) {
    char *name = malloc(MAXNAME);
    char *tk = strtok(args, " ");
    if (!tk) { free(name); Error("handle_w: Invalid arguments"); return 1; }
    strcpy(name, tk);
    tk = strtok(NULL, " ");
    if (!tk) { free(name); Error("handle_w: Missing length"); return 1; }
    uint len = atoi(tk);
    char *data = tk + strlen(tk) + 1;

    int rc = cmd_w(name, len, data);
    if (rc == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to write file");
    }
    free(name);
    return 0;
}

int handle_i(char *args) {
    char *name = malloc(MAXNAME);
    char *tk = strtok(args, " ");
    if(tk == NULL){
        free(name);
        Error("handle i: Invalid arguments");
        return 1;
    }
    strcpy(name, tk);
    tk = strtok(NULL, " ");
    if(tk == NULL){
        free(name);
        Error("handle i: Invalid arguments");
        return 1;
    }
    uint pos = (tk ? atoi(tk) : 0);
    tk = strtok(NULL, " ");
    if(tk == NULL){
        free(name);
        Error("handle i: Invalid arguments");
        return 1;
    }
    uint len = (tk ? atoi(tk) : 0);
    
    char *data = tk ? tk + strlen(tk) + 1 : NULL;

    int rc = cmd_i(name, pos, len, data);
    if (rc == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to write file");
    }
    free(name);
    return 0;
}

int handle_d(char *args) {
    char *tk = strtok(args, " ");
    if (!tk) { Error("handle_d: Invalid arguments"); return 1; }
    char *name = malloc(MAXNAME);
    strcpy(name, tk);
    tk = strtok(NULL, " ");
    if (!tk) { free(name); Error("handle_d: Missing position"); return 1; }
    uint pos = atoi(tk);
    tk = strtok(NULL, " ");
    if (!tk) { free(name); Error("handle_d: Missing length"); return 1; }
    uint len = atoi(tk);
    if (cmd_d(name, pos, len) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to delete file");
    }
    free(name);
    return 0;
}

int handle_e(char *args) {
    printf("Bye!\n");
    Log("Exit");
    return -1;
}

int handle_login(char *args) {
    char *tk = strtok(args, " ");
    if (!tk) { Error("handle_login: Invalid arguments"); return 1; }
    int uid = atoi(tk);
    if (cmd_login(uid) == E_SUCCESS) {
        ReplyYes();
    } else {
        ReplyNo("Failed to login");
    }
    return 0;
}

static struct {
    const char *name;
    int (*handler)(char *);
} cmd_table[] = {{"f", handle_f},        {"mk", handle_mk},       {"mkdir", handle_mkdir}, {"rm", handle_rm},
                 {"cd", handle_cd},      {"rmdir", handle_rmdir}, {"ls", handle_ls},       {"cat", handle_cat},
                 {"w", handle_w},        {"i", handle_i},         {"d", handle_d},         {"e", handle_e},
                 {"login", handle_login}};

#define NCMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

FILE *log_file;

int main(int argc, char *argv[]) {
    log_init("fs.log");

    // assert(BSIZE % sizeof(dinode) == 0);

    // get disk info and store in global variables
    get_disk_info(&ncyl, &nsec);

    // read the superblock
    sbinit();

    static char buf[4096];
    while (1) {
        fgets(buf, sizeof(buf), stdin);
        if (feof(stdin)) break;
        buf[strlen(buf) - 1] = 0;
        Log("Use command: %s", buf);
        char *p = strtok(buf, " ");
        int ret = 1;
        for (int i = 0; i < NCMD; i++)
            if (p && strcmp(p, cmd_table[i].name) == 0) {
                ret = cmd_table[i].handler(p + strlen(p) + 1);
                break;
            }
        if (ret == 1) {
            Log("No such command");
            printf("No\n");
        }
        if (ret < 0) break;
    }

    log_close();
}
