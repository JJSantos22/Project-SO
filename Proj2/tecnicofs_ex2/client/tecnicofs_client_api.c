#include "tecnicofs_client_api.h"
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int tx;
int rx;
int id_session;
char client_pipe[MAXPIPENAME];

int tfs_mount(char const *client_pipe_path, char const *server_pipe_path) {
    unlink(client_pipe_path);
    if (mkfifo(client_pipe_path, 0666) == -1){
        return -1;
    }
    strcpy(client_pipe, client_pipe_path);
    tx = open(server_pipe_path, O_WRONLY);
    if(tx == -1){
        return -1;
    }
    char *buffer=(char*) malloc(sizeof(int)+MAXPIPENAME*sizeof(char));
    memset(buffer, TFS_OP_CODE_MOUNT, 1);
    memset(buffer+sizeof(int), '\0', MAXPIPENAME*sizeof(char));
    memcpy(buffer+sizeof(int), client_pipe_path, strlen(client_pipe_path));
    ssize_t ret = write(tx, buffer, sizeof(int)+MAXPIPENAME*sizeof(char));
    free(buffer);
    if (ret==-1){
        return -1;
    }
    rx = open(client_pipe_path, O_RDONLY);
    if(rx == -1){
        return -1;
    }
    ret = read(rx, &id_session, sizeof(int));
    if (ret==-1){
        return -1;
    }
    return 0;
}

int tfs_unmount() {
    char *buffer=(char*) malloc(sizeof(int)*2);
    memset(buffer, TFS_OP_CODE_UNMOUNT, 1);
    memset(buffer+sizeof(int), id_session, 1);
    ssize_t ret = write(tx, buffer, sizeof(int)*2);
    free(buffer);
    if (ret==-1)
        return -1;
    close(tx);
    close(rx);
    unlink(client_pipe);
    return 0;
}

int tfs_open(char const *name, int flags) {
    char *buffer=(char*) malloc(MAXFILENAME*sizeof(char) +sizeof(int)*3);
    memset(buffer, TFS_OP_CODE_OPEN, 1);
    memset(buffer+sizeof(int), id_session, 1);
    memset(buffer+sizeof(int)*2, '\0', MAXFILENAME*sizeof(char));
    memcpy(buffer+(sizeof(int)*2), name, strlen(name)*sizeof(char));
    memset(buffer+(sizeof(int)*2)+MAXFILENAME*sizeof(char), flags, 1);
    ssize_t ret = write(tx, buffer, MAXFILENAME*sizeof(char) + (sizeof(int)*3));
    free(buffer);
    if (ret==-1){
        return -1;
    } 
    int fhandle;
    ret = read(rx, &fhandle, sizeof(int));
    if (ret==-1){
        return -1;
    }
    return fhandle;
} 

int tfs_close(int fhandle) {
    char *buffer=(char*) malloc(sizeof(int)*3);
    memset(buffer, TFS_OP_CODE_CLOSE, 1);
    memset(buffer+sizeof(int), id_session, 1);
    memset(buffer+(sizeof(int)*2), fhandle, 1);
    ssize_t ret = write(tx, buffer, sizeof(int)*3);
    free(buffer);
    if (ret==-1){
        return -1;
    } 
    int flag;
    ret = read(rx, &flag, sizeof(int));
    if (ret==-1){
        return -1;
    }
    return flag;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t len) {
    char *buffertotal=(char*) malloc(strlen(buffer)*sizeof(char) + sizeof(int)*3 + sizeof(len));
    memset(buffertotal, TFS_OP_CODE_WRITE, 1);
    memset(buffertotal+sizeof(int), id_session, 1);
    memset(buffertotal+(sizeof(int)*2), fhandle, 1);
    memcpy(buffertotal+(sizeof(int)*3), &len, sizeof(len));
    memcpy(buffertotal + sizeof(int)*3 + sizeof(len), buffer, strlen(buffer)*sizeof(char));
    ssize_t ret = write(tx, buffertotal, strlen(buffer)*sizeof(char) + sizeof(int)*3 + sizeof(len));
    free(buffertotal);
    if (ret==-1){
        return -1;
    } 
    int bytes;
    ret = read(rx, &bytes, sizeof(int));
    if (ret==-1){
        return -1;
    }
    return (size_t)bytes;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    char *buffertotal=(char*) malloc(sizeof(int)*3 + sizeof(len));
    memset(buffertotal, TFS_OP_CODE_READ, 1);
    memset(buffertotal+sizeof(int), id_session, 1);
    memset(buffertotal+(sizeof(int)*2), fhandle, 1);
    memcpy(buffertotal+(sizeof(int)*3), &len, sizeof(len));
    ssize_t ret = write(tx, buffertotal, sizeof(int)*3 + sizeof(len));
    free(buffertotal);
    if (ret==-1){
        return -1;
    } 
    int bytes;
    ret = read(rx, &bytes, sizeof(int));
    if (ret==-1 || bytes==-1){
        return -1;
    }
    ret = read(rx, buffer, sizeof(char)*(size_t)bytes);
    if (ret==-1){
        return -1;
    }
    return (size_t)bytes;
}

int tfs_shutdown_after_all_closed() {
    char *buffer=(char*) malloc(sizeof(int)*2);
    memset(buffer, TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED, 1);
    memset(buffer+sizeof(int), id_session, 1);
    ssize_t ret = write(tx, buffer, sizeof(int)*2);
    free(buffer);
    if (ret==-1){
        return -1;
    } 
    int flag;
    ret = read(rx, &flag, sizeof(int));
    if (ret==-1){
        return -1;
    }
    return flag;
}
