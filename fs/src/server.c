#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../include/log.h"
#include "../../include/tcp_buffer.h"
#include "../../include/tcp_utils.h"
#include "../include/fs.h"
#include "../include/common.h"
#include "assert.h"
#define BUFSIZE 1024

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t serverLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t reader, writer;

int reader_cnt = 0;
struct Mapping{
    int client_id;
    int uid; //file system user id
}users_map[MAXUSERS];

int handle_f(tcp_buffer *wb, int argc, char *args[], char *reply){
    // writer lock
    pthread_mutex_lock(&reader);
    pthread_mutex_lock(&writer);

    static char buf[BUFSIZE];
    if(argc != 1){
        sprintf(buf, "Usage: f (a single f)\n");
        Error("format : Invalid arguments");
        reply_with_no(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return -1;
    }
    assert(strcmp(args[0], "f") == 0);
    int ret = cmd_f();
    if(ret != E_SUCCESS){
        sprintf(buf, "format : format failed, only Root user can format\n");
        Error("format : Failed to format");
        reply_with_no(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return -1;
    }else{
        sprintf(buf, "format : Success\n");
        Log("format : Success");
        reply_with_yes(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return 0;
    }
    return 0;
}

int handle_mk(tcp_buffer *wb, int argc, char *args[], char *reply){
    // mk f [mode]: create a file with the given name and mode
    // writer lock
    pthread_mutex_lock(&reader);
    pthread_mutex_lock(&writer);

    static char buf[BUFSIZE];
    if(argc != 2 && argc != 3){
        sprintf(buf, "Usage: mk <filename> [mode]\n");
        Error("mk : Invalid arguments");
        reply_with_no(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return -1;
    }

    char *name = args[1];
    short mode = (argc == 3) ? atoi(args[2]) : 0b111111;

    int ret = cmd_mk(name, mode);
    if(ret != E_SUCCESS){
        sprintf(buf, "mk : Failed to create file\n");
        Error("mk : Failed to create file");
        reply_with_no(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return -1;
    }else{
        sprintf(buf, "mk : Success\n");
        Log("mk : Success");
        reply_with_yes(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return 0;
    }
}

int handle_mkdir(tcp_buffer *wb, int argc, char *args[], char *reply){
    // mkdir dirname [mode]: create a directory with the given name and mode.
    //some writing operations are needed
    // writer lock
    pthread_mutex_lock(&reader);
    pthread_mutex_lock(&writer);

    static char buf[BUFSIZE];
    if(argc != 2 && argc != 3){
        sprintf(buf, "Usage: mkdir <dirname> [mode]\n");
        Error("mkdir : Invalid arguments, %d", argc);
        reply_with_no(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return -1;
    }

    char *name = args[1];
    short mode = (argc == 3) ? atoi(args[2]) : 0b111111;

    int ret = cmd_mkdir(name, mode);
    if(ret != E_SUCCESS){
        sprintf(buf, "mkdir : Failed to create directory\n");
        Error("mkdir : Failed to create directory");
        reply_with_no(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return -1;
    }else{
        sprintf(buf, "mkdir : Success\n");
        Log("mkdir : Success");
        reply_with_yes(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return 0;
    }
}

int handle_rm(tcp_buffer *wb, int argc, char *args[], char *reply){
    // writer lock
    pthread_mutex_lock(&reader);
    pthread_mutex_lock(&writer);

    char buf[BUFSIZE];
    if(argc != 2){
        sprintf(buf, "Usage: rm <filename>\n");
        Error("rm : Invalid arguments");
        reply_with_no(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return -1;
    }

    char *name = args[1];

    int ret = cmd_rm(name);
    if(ret != E_SUCCESS){
        sprintf(buf, "rm : Failed to remove file\n");
        Error("rm : Failed to remove file");
        reply_with_no(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return -1;
    }else{
        sprintf(buf, "rm : Success\n");
        Log("rm : Success");
        reply_with_yes(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return 0;
    }
}

int handle_cd(tcp_buffer *wb, int argc, char *args[], char *reply){
    // A reader
    pthread_mutex_lock(&writer);
    pthread_mutex_lock(&mutex);
    reader_cnt++;
    pthread_mutex_unlock(&mutex);
    if(reader_cnt == 1){
        pthread_mutex_lock(&reader);
    }


    char buf[BUFSIZE];
    if(argc != 2){
        sprintf(buf, "Usage: cd <dirname>\n");
        Error("cd : Invalid arguments");
        reply_with_no(wb, buf, strlen(buf) + 1);

        pthread_mutex_lock(&mutex);
        reader_cnt--;
        pthread_mutex_unlock(&mutex);
        if(reader_cnt == 0){
            pthread_mutex_unlock(&reader);
        }
        pthread_mutex_unlock(&writer);

        return -1;
    }

    char *name = args[1];

    int ret = cmd_cd(name);
    if(ret != E_SUCCESS){
        sprintf(buf, "cd : Failed to change directory\n");
        Error("cd : Failed to change directory");
        reply_with_no(wb, buf, strlen(buf) + 1);


        pthread_mutex_lock(&mutex);
        reader_cnt--;
        pthread_mutex_unlock(&mutex);
        if(reader_cnt == 0){
            pthread_mutex_unlock(&reader);
        }
        pthread_mutex_unlock(&writer);

        return -1;
    }else{
        sprintf(buf, "cd : Success\n");
        Log("cd : Success");
        reply_with_yes(wb, buf, strlen(buf) + 1);

        pthread_mutex_lock(&mutex);
        reader_cnt--;
        pthread_mutex_unlock(&mutex);
        if(reader_cnt == 0){
            pthread_mutex_unlock(&reader);
        }
        pthread_mutex_unlock(&writer);

        return 0;
    }
}

int handle_rmdir(tcp_buffer *wb, int argc, char *args[], char *reply){
    // writer lock
    pthread_mutex_lock(&reader);
    pthread_mutex_lock(&writer);
    char buf[BUFSIZE];
    if(argc != 2){
        sprintf(buf, "Usage: rmdir <dirname>\n");
        Error("rmdir : Invalid arguments");
        reply_with_no(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return -1;
    }

    char *name = args[1];

    int ret = cmd_rmdir(name);
    if(ret != E_SUCCESS){
        sprintf(buf, "rmdir : Failed to remove directory\n");
        Error("rmdir : Failed to remove directory");
        reply_with_no(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return -1;
    }else{
        sprintf(buf, "rmdir : Success\n");
        Log("rmdir : Success");
        reply_with_yes(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return 0;
    }
}

int handle_ls(tcp_buffer *wb, int argc, char *args[], char *reply){
    // reader lock
    pthread_mutex_lock(&writer);
    pthread_mutex_lock(&mutex);
    reader_cnt++;
    pthread_mutex_unlock(&mutex);
    if(reader_cnt == 1) pthread_mutex_lock(&reader);
    char buf[BUFSIZE];
    if(argc != 1){
        sprintf(buf, "Usage: ls\n");
        Error("ls : Invalid arguments");
        reply_with_no(wb, buf, strlen(buf) + 1);
        // unlock on failure
        pthread_mutex_lock(&mutex);
        reader_cnt--;
        pthread_mutex_unlock(&mutex);
        if(reader_cnt == 0) pthread_mutex_unlock(&reader);
        pthread_mutex_unlock(&writer);
        return -1;
    }
    entry *entries = NULL;
    int n = 0;
    int ret = cmd_ls(&entries, &n);
    if(ret != E_SUCCESS){
        sprintf(buf, "ls : Failed to list files\n");
        Error("ls : Failed to list files");
        reply_with_no(wb, buf, strlen(buf) + 1);
        // unlock on failure
        pthread_mutex_lock(&mutex);
        reader_cnt--;
        pthread_mutex_unlock(&mutex);
        if(reader_cnt == 0) pthread_mutex_unlock(&reader);
        pthread_mutex_unlock(&writer);
        return -1;
    }else{
        sprintf(buf, "total entries: %d \n", n);
        for(int i=0;i<n;i++){
            sprintf(buf + strlen(buf), "%s\t", entries[i].name);
            sprintf(buf + strlen(buf), "Type: %s\n", entries[i].type == T_FILE ? "File" : "Directory");
        }
        Log("ls : Success");
        reply_with_yes(wb, buf, strlen(buf) + 1);
        // unlock on success
        pthread_mutex_lock(&mutex);
        reader_cnt--;
        pthread_mutex_unlock(&mutex);
        if(reader_cnt == 0) pthread_mutex_unlock(&reader);
        pthread_mutex_unlock(&writer);
        return 0;
    }
}

int handle_cat(tcp_buffer *wb, int argc, char *args[], char *reply){
    // reader lock
    pthread_mutex_lock(&writer);
    pthread_mutex_lock(&mutex);
    reader_cnt++;
    pthread_mutex_unlock(&mutex);
    if(reader_cnt == 1) pthread_mutex_lock(&reader);
    char buf[BUFSIZE];
    if(argc != 2){
        sprintf(buf, "Usage: cat <filename>\n");
        Error("cat : Invalid arguments");
        reply_with_no(wb, buf, strlen(buf) + 1);
        // unlock on failure
        pthread_mutex_lock(&mutex);
        reader_cnt--;
        pthread_mutex_unlock(&mutex);
        if(reader_cnt == 0) pthread_mutex_unlock(&reader);
        pthread_mutex_unlock(&writer);
        return -1;
    }

    char name[MAXNAME];
    strcpy(name, args[1]);

    uchar *data = NULL;  
    uint len = 0;

    int ret = cmd_cat(name, &data, &len);
    if(ret != E_SUCCESS){
        sprintf(buf, "cat : Failed to read file\n");
        Error("cat : Failed to read file");
        reply_with_no(wb, buf, strlen(buf) + 1);
        // unlock on failure
        pthread_mutex_lock(&mutex);
        reader_cnt--;
        pthread_mutex_unlock(&mutex);
        if(reader_cnt == 0) pthread_mutex_unlock(&reader);
        pthread_mutex_unlock(&writer);
        return -1;
    }else{
        Log("cat : Success");
        memcpy(buf, data, min(len, BUFSIZE));
        buf[min(len, BUFSIZE)] = '\0'; // null-terminate the string

        reply_with_yes(wb, buf, len);
        free(data);
        // unlock on success
        pthread_mutex_lock(&mutex);
        reader_cnt--;
        pthread_mutex_unlock(&mutex);
        if(reader_cnt == 0) pthread_mutex_unlock(&reader);
        pthread_mutex_unlock(&writer);
        return 0;
    }
}

int handle_w(tcp_buffer *wb, int argc, char *args[], char *reply){
    // writer lock
    pthread_mutex_lock(&reader);
    pthread_mutex_lock(&writer);
    char buf[BUFSIZE];
    if(argc != 3 && argc != 4){
        sprintf(buf, "Usage: w <filename> <length> [data]\n");
        Error("w : Invalid arguments");
        reply_with_no(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return -1;
    }

    char name[MAXNAME];
    strcpy(name, args[1]);
    uint len = atoi(args[2]);
    char *data = (argc == 4) ? args[3] : NULL;

    int ret = cmd_w(name, len, data);
    if(ret != E_SUCCESS){
        sprintf(buf, "w : Failed to write file\n");
        Error("w : Failed to write file");
        reply_with_no(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return -1;
    }else{
        sprintf(buf, "w : Success\n");
        Log("w : Success");
        reply_with_yes(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return 0;
    }
}

int handle_i(tcp_buffer *wb, int argc, char *args[], char *reply){
    // writer lock
    pthread_mutex_lock(&reader);
    pthread_mutex_lock(&writer);
    char buf[BUFSIZE];
    if(argc != 5){
        sprintf(buf, "Usage: i <filename> <position> <length> <data>\n");
        Error("i : Invalid arguments");
        reply_with_no(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return -1;
    }
    char name[MAXNAME];
    strcpy(name, args[1]);
    uint pos = atoi(args[2]);
    uint len = atoi(args[3]);
    char *data = args[4];

    int ret = cmd_i(name, pos, len, data);

    if(ret != E_SUCCESS){
        sprintf(buf, "i : Failed to insert data\n");
        Error("i : Failed to insert data");
        reply_with_no(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return -1;
    }else{
        sprintf(buf, "i : Success\n");
        Log("i : Success");
        reply_with_yes(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return 0;
    }    
}

int handle_d(tcp_buffer *wb, int argc, char *args[], char *reply){
    // writer lock
    pthread_mutex_lock(&reader);
    pthread_mutex_lock(&writer);
    char buf[BUFSIZE];
    if(argc != 4){
        sprintf(buf, "Usage: d <filename> <position> <length>\n");
        Error("d : Invalid arguments");
        reply_with_no(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return -1;
    }

    char name[MAXNAME];
    strcpy(name, args[1]);
    uint pos = atoi(args[2]);
    uint len = atoi(args[3]);

    int ret = cmd_d(name, pos, len);
    if(ret != E_SUCCESS){
        sprintf(buf, "d : Failed to delete data\n");
        Error("d : Failed to delete data");
        reply_with_no(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return -1;
    }else{
        sprintf(buf, "d : Success\n");
        Log("d : Success");
        reply_with_yes(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return 0;
    }
}

int handle_e(tcp_buffer *wb, int argc, char *args[], char *reply){
    // writer lock
    pthread_mutex_lock(&reader);
    pthread_mutex_lock(&writer);
    char buf[BUFSIZE];
    if(argc != 2){
        sprintf(buf, "Usage: exit <uid>");
        Error("e : Invalid arguments");
        reply_with_no(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return -1;
    }
    for(int i=0;i<MAXUSERS;i++){
        if(users_map[i].uid == atoi(args[1])){
            users_map[i].uid = -1; // reset uid
            Log("User %d logged out", atoi(args[1]));
            break;
        }
    }
    cmd_exit(atoi(args[1]));
    sprintf(buf, "Bye!\n");
    Log("Exit");
    reply_with_yes(wb, buf, strlen(buf) + 1);
    pthread_mutex_unlock(&writer);
    pthread_mutex_unlock(&reader);
    return 0;
}

int handle_login(tcp_buffer *wb, int argc, char *args[], char *reply){
    // writer lock
    pthread_mutex_lock(&reader);
    pthread_mutex_lock(&writer);
    char buf[BUFSIZE];
    if(argc != 2){
        sprintf(buf, "Usage: login <uid>\n");
        Error("login : Invalid arguments");
        reply_with_no(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return -1;
    }

    int uid = atoi(args[1]);

    int ret = cmd_login(uid);
    if(ret != E_SUCCESS){
        sprintf(buf, "login : Failed to login\n");
        Error("login : Failed to login");
        reply_with_no(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return -1;
    }else{
        sprintf(buf, "login : Success\n");
        Log("login : Success");
        reply_with_yes(wb, buf, strlen(buf) + 1);
        pthread_mutex_unlock(&writer);
        pthread_mutex_unlock(&reader);
        return 0;
    }
}

int handle_pwd(tcp_buffer *wb, int argc, char *args[], char *reply){
    // reader lock
    pthread_mutex_lock(&writer);
    pthread_mutex_lock(&mutex);
    reader_cnt++;
    pthread_mutex_unlock(&mutex);
    if(reader_cnt == 1) pthread_mutex_lock(&reader);
    char *buf;
    int ret = cmd_pwd(&buf, BSIZE);
    if (ret != E_SUCCESS) {
        reply_with_no(wb, "pwd failed\n", 11);
        free(buf);
        // unlock reader
        pthread_mutex_lock(&mutex);
        reader_cnt--;
        pthread_mutex_unlock(&mutex);
        if(reader_cnt == 0) pthread_mutex_unlock(&reader);
        pthread_mutex_unlock(&writer);
        return 0;
    } else {
        reply_with_yes(wb, buf, strlen(buf)+1);
        free(buf);
        // unlock reader
        pthread_mutex_lock(&mutex);
        reader_cnt--;
        pthread_mutex_unlock(&mutex);
        if(reader_cnt == 0) pthread_mutex_unlock(&reader);
        pthread_mutex_unlock(&writer);
        return  -1;
    }
}    

static struct {
    const char *name;
    int (*handler)(tcp_buffer *wb, int argc, char *args[], char *reply);
} cmd_table[] = {{"f", handle_f},        {"mk", handle_mk},       {"mkdir", handle_mkdir}, {"rm", handle_rm},
                 {"cd", handle_cd},      {"rmdir", handle_rmdir}, {"ls", handle_ls},       {"cat", handle_cat},
                 {"w", handle_w},        {"i", handle_i},         {"d", handle_d},         {"e", handle_e},
                 {"login", handle_login}, {"pwd", handle_pwd}};

#define NCMD (sizeof(cmd_table) / sizeof(cmd_table[0]))



void on_connection(int id) {
    // some code that are executed when a new client is connected
    pthread_mutex_lock(&serverLock);
    for(int i =0; i<MAXUSERS ;i++){
        if(users_map[i].client_id == -1){
            users_map[i].client_id = id;
            users_map[i].uid = -1; // -1 means not logged in
            break;
        }
    }
    Log("on_connection: client %d connected", id);
    pthread_mutex_unlock(&serverLock);
}

int fetch_uid(int id){
    uint ret = 0;
    for(int i = 0; i<MAXUSERS ;i++){
        if(users_map[i].client_id == id){
            ret = users_map[i].uid;
            return ret;
        }
    }
    fprintf(stderr, "client Not logged in\n");
    return -1;
}

int on_recv(int id, tcp_buffer *wb, char *msg, int len) {
    int client_uid = fetch_uid(id);

    // 1. 去掉末尾的换行符
    if (len > 0 && msg[len - 1] == '\n') {
        msg[len - 1] = '\0';
    }

    // 2. 分割命令和参数
    int argc = 0;
    char *argv[150];
    char *token;

    int accLen = 0;
    while(argc < 150){
        if(argc == 3 && strcmp(argv[0], "w") == 0) {
            int data_len = 0;
            if(len - accLen < atoi(argv[2]) + 1) {
                fprintf(stderr, "Data length %s exceeds remaining message length %d\n", argv[2], len - accLen);
                Error("on recv: Data length exceeds remaining message length");
                reply_with_no(wb, "on recv: Data length exceeds remaining message length\n", 50);
                return 0;
            }
            data_len = atoi(argv[2]); // get the length of data
            assert(data_len >= 0);
            char *rest = token + strlen(token) + 1; // skip the first space;
            rest[data_len] = '\0'; // ensure the data is null-terminated
            argv[argc++] = rest; // store the data as the last argument
            fprintf(stderr, "Data length: %d, Data: %s\n", data_len, rest);
            break; // no more tokens to process
        }else if(argc == 4 && strcmp(argv[0], "i") == 0) {
            int data_len = 0;
            if(len - accLen <atoi(argv[3]) + 1) {
                fprintf(stderr, "Data length %s exceeds remaining message length %d\n", argv[3], len - accLen);
                Error("on recv: commmad i Data length exceeds remaining message length");
                reply_with_no(wb, "on recv: Data length exceeds remaining message length\n", 50);
                return 0;
            }
            data_len = atoi(argv[3]);
            assert(data_len >= 0);

            //i f pos l data: Insert data to a file.
            char *rest = token + strlen(token) + 1; // skip the first space;
            rest[data_len] = '\0'; // ensure the data is null-terminated
            argv[argc++] = rest; // store the data as the last argument
            fprintf(stderr, "Data length: %d, Data: %s\n", data_len, rest);
            break; // no more tokens to process
        } else {
            if(argc == 0) token = strtok(msg, " \r\n"); // first token is the command
            else token = strtok(NULL, " \r\n");
            if(token == NULL) {
                break; // no more tokens to process
            }            
            argv[argc++] = token;
            accLen += strlen(token) + 1; // +1 for the space or null terminator
        }

    }
    // 如果没有任何参数，直接返回
    if (argc == 0) {
        reply_with_no(wb, "No command received\n", 20);
        return 0;
    }

    // Reject login without UID to prevent crash
    if (strcmp(argv[0], "login") == 0 && argc < 2) {
        reply_with_no(wb, "Usage: login <uid>\n", strlen("Usage: login <uid>\n") + 1);
        return 0;
    }

    // 打印收到的命令
    fprintf(stderr, "Received command: %s\n", argv[0]);

    if(client_uid < 0 && strcmp(argv[0], "login") != 0){
        const char err[] = "Please login first";
        reply_with_no(wb, err, strlen(err) + 1);
        return 0;
    }


    // 3. 查表分发
    char *none;
    int ret = 1;  

    // if user tries 'login' without uid, reject early
    if (strcmp(argv[0], "login") == 0 && argc < 2) {
        reply_with_no(wb, "Usage: login <uid>\n", strlen("Usage: login <uid>\n") + 1);
        return 0;
    }

    if (strcmp(argv[0], "login") == 0) {
        // perform login, handle_usage covers argc check
        ret = handle_login(wb, argc, argv, none);
        // on successful login (ret==0) and correct args, update mapping
        if (ret == 0 && argc >= 2) {
            for (int j = 0; j < MAXUSERS; j++) {
                if (users_map[j].client_id == id) {
                    users_map[j].uid = atoi(argv[1]);
                    break;
                }
            }
        }

        return 0;
    }
    
    if(strcmp(argv[0],"f") == 0){
        if(client_uid != 1){
            const char err[] = "Only root can format the file system";
            reply_with_no(wb, err, strlen(err) + 1);
            return 0;
        }

        ret = handle_f(wb, argc, argv, none);

        return 0;
    }

    for (int i = 0; i < NCMD; i++) {
        if (strcmp(argv[0], cmd_table[i].name) == 0) {
            if(is_formated() == false){
                const char err[] = "File system not formated, formate first";
                reply_with_no(wb, err, strlen(err) + 1);

                return 0;
            }
            user_init(client_uid);
            ret = cmd_table[i].handler(wb, argc, argv, none);
            user_end(client_uid);

            return 0;
        }
    }

    char err[BUFSIZE];
    sprintf(err, "Unknown command: %s\n", argv[0]);
    reply_with_no(wb, err, strlen(err) + 1);

    return 0;
}

void cleanup(int id) {
    pthread_mutex_lock(&serverLock);
    // some code that are executed when a client is disconnected
    for(int i =0; i<MAXUSERS ;i++){
        if(users_map[i].client_id == id){
            users_map[i].client_id = -1;
            for(int j=0;j<MAXUSERS;j++){
                if(sb.users[j].uid == users_map[i].uid){
                    sb.users[j].cwd = 0;
                    sb.users[j].uid = 0;
                    //擦除对应的所有用户信息
                    break;
                }
            }
            users_map[i].uid = -1; // wipe out information
            break;
        }
    }
    uchar *buf = (uchar *)malloc(BSIZE);
    memcpy(buf, &sb, sizeof(superblock));
    write_block(0, buf);
    free(buf);
    pthread_mutex_unlock(&serverLock);
}

FILE *log_file;

int main(int argc, char *argv[]) {
    log_init("fs.log");
    int FSPort=1145;
    if(argc != 4){
        fprintf(stderr, "Usage: ./FS <DiskServerAddress> <BDSPort=10356> <FSPort=12356>\n");
    } else {
        FSPort = atoi(argv[3]);
        // initialize disk-server address and port from arguments
        BDS_port = atoi(argv[2]);
        strncpy(BDS_addr, argv[1], sizeof(BDS_addr) - 1);
        BDS_addr[sizeof(BDS_addr) - 1] = '\0';
        if(FSPort <= 0){
            fprintf(stderr, "Invalid port number\n");
            return -1;
        }
    }
    for(int i=0;i<MAXUSERS;i++){
        users_map[i].client_id = -1;
        users_map[i].uid = -1;
    }
    diskClientSetup();
    load_basic_data(); // load superblock and bitmap from disk
    
    tcp_server server = server_init(FSPort, 20, on_connection, on_recv, cleanup);
        //端口号, 线程数, 传入三个函数, 分别是: 有一个客户端连接时执行, server收到客户端的消息时的处理函数, 客户端断开连接时的处理函数
    server_run(server);
    // _mount_disk();

    free(sb.bitmap);
    log_close();
}
