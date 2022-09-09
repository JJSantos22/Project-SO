#include "fs/operations.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>

#define SIZE 256
#define N 1023

typedef struct {
  char *path;
  void *buffer;
} Info;

void *sWrite(void *args) {
    Info argm = *((Info*)args);
    char *path = argm.path;
    char *buffer = argm.buffer;
    int fhandle = tfs_open(path, TFS_O_CREAT);
    assert(fhandle != -1);
    assert((tfs_write(fhandle, buffer, SIZE))==SIZE);
    assert(tfs_close(fhandle)!=-1);
    return NULL;
}

void *sRead(void *args) {
    Info argm = *((Info*)args);
    char *path = argm.path;
    char *buffer = argm.buffer;
    int fhandle = tfs_open(path, 0);
    assert(fhandle != -1);
    assert((tfs_read(fhandle, buffer, SIZE))==SIZE);
    assert(tfs_close(fhandle)!=-1);
    return NULL;
}


int main() {
  
    pthread_t tid[2*N];
    char *path = "/f1";

    Info arg1;
    char buffer[SIZE];

    Info arg2;
    char buffer2[SIZE];
  
    memset(buffer, 'A', SIZE);

    arg1.path = path;
    arg1.buffer = buffer;

    arg2.path = path;
    arg2.buffer = buffer2;
  
    assert(tfs_init() != -1);

    for(int i=0; i< N; i++){
        pthread_create(&tid[i], NULL, sWrite, &arg1);
    }
    for (int i=N; i< 2*N; i++){
        pthread_create(&tid[i], NULL, sRead, &arg2);
    }

    for (int i=0; i<2*N; i++)
        pthread_join (tid[i], NULL);
    //compares first 256 characters
    assert(memcmp(buffer,buffer2,SIZE)==0);

    tfs_destroy();
    printf("Successful test\n");
    return 0;
}
