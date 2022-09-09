#include "client/tecnicofs_client_api.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char **argv) {

    char *path1="aaa1";
    char *path2="aaa2";
    char *path3="aaa3";
    char *path4="aaa4";
    char *path5="aaa5";
    char *path6="aaa6";
    char *path7="aaa7";
    char *path8="aaa8";
    char *path9="aaa9";
    int f;

    if (argc < 3) {
        printf("You must provide the following arguments: 'client_pipe_path "
               "server_pipe_path'\n");
        return 1;
    }

    assert(tfs_mount(argv[1], argv[2]) == 0);

    f = tfs_open(path1, TFS_O_CREAT);
        assert(f != -1);
    f = tfs_open(path2, TFS_O_CREAT);
        assert(f != -1);
    f = tfs_open(path3, TFS_O_CREAT);
        assert(f != -1);
    f = tfs_open(path4, TFS_O_CREAT);
        assert(f != -1);
    f = tfs_open(path5, TFS_O_CREAT);
        assert(f != -1);
    f = tfs_open(path6, TFS_O_CREAT);
        assert(f != -1);
    f = tfs_open(path7, TFS_O_CREAT);
        assert(f != -1);
    f = tfs_open(path8, TFS_O_CREAT);
        assert(f != -1);
    f = tfs_open(path9, TFS_O_CREAT);
        assert(f != -1);

    assert(tfs_shutdown_after_all_closed() == 0);

    for (int i=0; i<9; i++){
        assert(tfs_close(i) != -1);
    }

    printf("Successful test.\n");

    return 0;
}


