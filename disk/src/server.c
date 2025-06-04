#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "assert.h"
#include "../include/disk.h"
#include "../../include/log.h"
#include "../../include/tcp_utils.h"

#define ARG_MAX 16

int parse(char *line, char *cmd[], int argc) {
    char *p = strtok(line, " ");
    int idx = 0;
    while (p) {
        cmd[idx++] = p;
        if (idx == argc) break;
        p = strtok(NULL, " ");
    }
    return idx == argc;
}


int string_to_dec(char *str, int *num) {
    char *endptr;
    *num = strtol(str, &endptr, 10);
    return *endptr == '\0';
}

int handle_i(tcp_buffer *wb, char *args, int len) {
    Log("Info command");
    int ncyl, nsec;
    cmd_i(&ncyl, &nsec);
    static char buf[64];
    sprintf(buf, "%d %d", ncyl, nsec);

    // including the null terminator
    reply(wb, buf, strlen(buf) + 1);
    return 0;
}

int handle_r(tcp_buffer *wb, char *args, int len) {
    Log("Read command");
    char *cmd[ARG_MAX];
    memset(cmd, 0, sizeof(cmd));
    assert(len>=0);
    
    printf("line: %s, len:%d\n", args, len);
    int ret = parse(args, cmd, 2);
    if (ret == 0) {
        return 0;
    }
    int cyl, sec;
    if (!(string_to_dec(cmd[0], &cyl) && string_to_dec(cmd[1], &sec))) {
        return 0;
    }
    char buf[512];

    if (cmd_r(cyl, sec, buf) == 0) {
        printf("read\n");
        reply_with_yes(wb, buf, 512);
    } else {
        reply_with_no(wb, NULL, 0);
    }
    return 0;
}

int handle_w(tcp_buffer *wb, char *args, int len) {
    Log("Write command");
    char *cmd[ARG_MAX];
    memset(cmd, 0, sizeof(cmd));
    printf("line: %s, len:%d\n", args, len);
    assert(len >= 0);
    int ret = parse(args, cmd, 3);
    if (ret == 0) {
        printf("No\n");
        return 0;
    }
    int cyl, sec, datalen;
    if (!(string_to_dec(cmd[0], &cyl) && string_to_dec(cmd[1], &sec) && string_to_dec(cmd[2], &datalen))) {
        printf("No\n");
        return 0;
    }
    char *data = cmd[2] + strlen(cmd[2]) + 1;

    if (cmd_w(cyl, sec, datalen, data) == 0) {
        printf("write\n");
        reply_with_yes(wb, NULL, 0);
    } else {
        reply_with_no(wb, NULL, 0);
    }
    return 0;
}

int handle_e(tcp_buffer *wb, char *args, int len) {
    const char *msg = "Bye!";
    reply(wb, msg, strlen(msg) + 1);
    return -1;
}

static struct {
    const char *name;
    int (*handler)(tcp_buffer *wb, char *, int);
} cmd_table[] = {
    {"I", handle_i},
    {"R", handle_r},
    {"W", handle_w},
    {"E", handle_e},
};

#define NCMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

void on_connection(int id) {
    // some code that are executed when a new client is connected
    // you don't need this now
}

int on_recv(int id, tcp_buffer *wb, char *msg, int len) {
    char *p = strtok(msg, " \r\n");
    // remove '\n\0' at the end of msg
    // now len doesnot include \0
    if (msg[len-2] == '\n') msg[len-2] = '\0';
    // len -= 2;
    int ret = 1;
    for (int i = 0; i < NCMD; i++)
        if (p && strcmp(p, cmd_table[i].name) == 0) {
            ret = cmd_table[i].handler(wb, p + strlen(p) + 1, len - strlen(p) - 1);
            break;
        }
    if (ret == 1) {
        static char unk[] = "Unknown command";
        buffer_append(wb, unk, sizeof(unk));
    }
    if (ret < 0) {
        return -1;
    }
    return 0;
}

void cleanup(int id) {
    // some code that are executed when a client is disconnected
    // you don't need this now
}

FILE *log_file;

int main(int argc, char *argv[]) {
    char *filename;
    int ncyl, nsec, ttd, port;
    if (argc < 5) {
        fprintf(stderr,
                "Usage: %s <disk file name> <cylinders> <sector per cylinder> "
                "<track-to-track delay> <port>\n",
                argv[0]);
        filename = (char *)malloc(64 * sizeof(char));
        strcpy(filename, "disk.img");
        ncyl = 100; // default number of cylinders
        nsec = 100; // default number of sectors per cylinder
        ttd = 10;  // default track-to-track delay
        port = 10356;
        fprintf(stderr, "Using default values: %s %d %d %d %d\n", filename, ncyl, nsec, ttd, port);
    }else{
        filename = argv[1];
        ncyl = atoi(argv[2]);
        nsec = atoi(argv[3]);
        ttd = atoi(argv[4]);
        port = atoi(argv[5]);
        if (ncyl <= 0 || nsec <= 0 || ttd < 0 || port <= 0) {
            fprintf(stderr, "Invalid arguments\n");
            exit(EXIT_FAILURE);
        }
    }   

    // args


    log_init("disk.log");

    int ret = init_disk(filename, ncyl, nsec, ttd);
    if (ret != 0) {
        fprintf(stderr, "Failed to initialize disk\n");
        exit(EXIT_FAILURE);
    }

    // command
    tcp_server server = server_init(port, 1, on_connection, on_recv, cleanup);
    server_run(server);

    // never reached
    close_disk();
    log_close();
    free(filename);
    return 0;
}
