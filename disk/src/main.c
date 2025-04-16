/*The final exe generated is BDS_local*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/disk.h"
#include "../../include/log.h"

// return a negative value to exit the program
int handle_i(char *args) {
    int ncyl, nsec;
    cmd_i(&ncyl, &nsec);
    printf("%d %d\n", ncyl, nsec);
    return 0;
}

int handle_r(char *args) {
    // Parse the arguments
    
    int cyl;
    int sec;
    char buf[512];

    char *token = strtok(args, " ");
    if(token == NULL){
        Log("Missing cylinder argument.");
        perror("Missing cylinder argument.");
        return  -1;
    }
    cyl = atoi(token);
    token = strtok(NULL, " ");
    if(token == NULL){
        Log("Missing sector argument.");
        perror("Missing sector argument.");
        return  -1;
    }
    sec = atoi(token);


    // Call the cmd_r function
    if (cmd_r(cyl, sec, buf) == 0) {
        printf("Yes\n");
        for (int i = 0; i < 512; i++) {
            printf("%c", buf[i]);
        }
        printf("\n");
    } else {
        printf("No\n");
    }
    return 0;
}

int handle_w(char *args) {
    int cyl = 0, sec = 0, len = 0;
    char *data = NULL;

    char *token = strtok(args, " ");
    if (!token) {
        fprintf(stderr, "Missing cylinder argument.\n");
        return -1;
    }
    cyl = atoi(token);

    token = strtok(NULL, " ");
    if (!token) {
        fprintf(stderr, "Missing sector argument.\n");
        return -1;
    }
    sec = atoi(token);

    token = strtok(NULL, " ");
    if (!token) {
        fprintf(stderr, "Missing length argument.\n");
        return -1;
    }
    len = atoi(token);

    // Get the remaining text—including any spaces—to copy 'len' bytes from
    token = strtok(NULL, "");
    if (!token) {
        fprintf(stderr, "Missing data.\n");
        return -1;
    }

    data = malloc(len + 1);
    if (!data) {
        perror("malloc");
        Log("Error allocating memory");
        return -1;
    }

    strncpy(data, token, len);

    if (cmd_w(cyl, sec, len, data) == 0) {
        printf("Yes\n");
    } else {
        printf("No\n");
    }

    free(data);
    return 0;
}

int handle_e(char *args) {
    printf("Bye!\n");
    return -1;
}

static struct {
    const char *name;
    int (*handler)(char *);
} cmd_table[] = {
    {"I", handle_i},
    {"R", handle_r},
    {"W", handle_w},
    {"E", handle_e},
};

#define NCMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

FILE *log_file;

int main(int argc, char *argv[]) {
    chdir("../runtimeCache");
    char fileName[256];
    int ncyl, nsec, ttd;
    if (argc < 4) {
        fprintf(stderr,
                "Usage: %s <disk file name> <cylinders> <sector per cylinder> "
                "<track-to-track delay>\n Run with default configuration...\n",
                argv[0]);
        strcpy(fileName, "毛主席万岁");
        ncyl = 10;
        nsec = 10;
        ttd = 10;
    }else{
        strcpy(fileName, argv[1]);
        ncyl = atoi(argv[2]);
        nsec = atoi(argv[3]);
        ttd = atoi(argv[4]);
        if (ncyl <= 0 || nsec <= 0 || ttd < 0) {
            fprintf(stderr, "Invalid arguments\n");
            exit(EXIT_FAILURE);
        }
    }



    log_init("disk.log");

    int ret = init_disk(fileName, ncyl, nsec, ttd);
    if (ret != 0) {
        fprintf(stderr, "Failed to initialize disk\n");
        exit(EXIT_FAILURE);
    }

    // command
    static char buf[1024];
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

    close_disk();
    log_close();
}
