#include "fs/operations.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>

#define SIZE 256

typedef struct {
  int fhandle;
  void *buffer;
} Info;

void *sWrite(void *args) {
    Info argm = *((Info*)args);
    int fhandle = argm.fhandle;
    char *buffer = argm.buffer;
    assert(fhandle != -1);
    assert((tfs_write(fhandle, buffer, SIZE))==SIZE);
    return NULL;
}

void *sRead(void *args) {
    Info argm = *((Info*)args);
    int fhandle = argm.fhandle;
    char *buffer = argm.buffer;
    assert(fhandle != -1);
    assert((tfs_read(fhandle, buffer, SIZE))==SIZE);
    return NULL;
}


int main() {
  
    pthread_t tid[3];
    char *path = "/f1";

    Info arg1;
    char buffer1[SIZE];

    Info arg2;
    char buffer2[SIZE];

    Info arg3;
    char buffer3[SIZE];
  
    memset(buffer1, 'A', SIZE);
    memset(buffer2, 'B', SIZE);
    memset(buffer3, 'C', SIZE);

    arg1.buffer = buffer1;

    arg2.buffer = buffer2;

    arg3.buffer = buffer3;
  
    assert(tfs_init() != -1);

    arg1.fhandle = tfs_open(path, TFS_O_CREAT);
    arg2.fhandle = tfs_open(path, TFS_O_APPEND);
    arg3.fhandle = tfs_open(path, TFS_O_APPEND);

    assert(arg1.fhandle!=arg2.fhandle && arg1.fhandle!=arg3.fhandle && arg2.fhandle!=arg3.fhandle);
    
    pthread_create(&tid[0], NULL, sWrite, &arg1);
    pthread_create(&tid[1], NULL, sWrite, &arg2);
    pthread_create(&tid[2], NULL, sWrite, &arg3);

    for(int e=0;e<3;e++){
        pthread_join(tid[e],NULL);
    }

    assert(tfs_close(arg1.fhandle)!=-1);
    assert(tfs_close(arg2.fhandle)!=-1);
    assert(tfs_close(arg3.fhandle)!=-1);

    arg1.fhandle = tfs_open(path, 0);
    arg2.fhandle = tfs_open(path, 0);
    arg3.fhandle = tfs_open(path, 0);

    pthread_create(&tid[0], NULL, sRead, &arg1);
    pthread_create(&tid[1], NULL, sRead, &arg2);
    pthread_create(&tid[2], NULL, sRead, &arg3);

    for(int e=0;e<3;e++){
        pthread_join(tid[e],NULL);
    }

    assert(tfs_close(arg1.fhandle)!=-1);
    assert(tfs_close(arg2.fhandle)!=-1);
    assert(tfs_close(arg3.fhandle)!=-1);


    assert(memcmp(buffer1,arg1.buffer,SIZE)==0);
    assert(memcmp(buffer2,arg2.buffer,SIZE)==0);
    assert(memcmp(buffer3,arg3.buffer,SIZE)==0);

    tfs_destroy();
    printf("Successful test\n");
    return 0;
}