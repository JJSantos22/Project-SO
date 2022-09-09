#include "fs/operations.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>

#define SIZE 256
#define N 20

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
  
    pthread_t tid[2];
    char *path = "/f1";

    Info arg1;
    char buffer1[SIZE];

    Info arg2;
    char buffer2[SIZE];

  
    memset(buffer1, 'A', SIZE);


    arg1.buffer = buffer1;

    arg2.buffer = buffer2;
  
    assert(tfs_init() != -1);

    arg1.fhandle = tfs_open(path, TFS_O_CREAT);
    arg2.fhandle = tfs_open(path, 0);

    assert(arg1.fhandle!=arg2.fhandle);
    
    pthread_create(&tid[0], NULL, sWrite, &arg1);
    pthread_create(&tid[1], NULL, sRead, &arg2);

   
    pthread_join(tid[0],NULL);
    pthread_join(tid[1],NULL);
    

    assert(tfs_close(arg1.fhandle)!=-1);
    assert(tfs_close(arg2.fhandle)!=-1);


    assert(memcmp(arg1.buffer,arg2.buffer,SIZE)==0);

    tfs_destroy();
    printf("Successful test\n");
    return 0;
}
