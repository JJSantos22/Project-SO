#include "operations.h"
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#define S 50 

void *worker(void *args);

typedef struct client{
    int id;
    char name[MAX_FILE_NAME];
    char *buffer;
    int flags;
    int fhandle;
    ssize_t len;
    int op;
    int tx;
    int op_on_the_way;
}Client;


static pthread_mutex_t list_lock;
static pthread_mutex_t user_Session[S];
static pthread_cond_t cond_Session[S];
pthread_t t[S];
int openedfiles[MAX_OPEN_FILES];
Client list[S];
int serverpipe;
char* pipename;

int main(int argc, char **argv) {
    int i,j;
    int tx;
    ssize_t ret;
    

    for (i = 0; i < S; i++){
        list[i].tx = -1;
        if (pthread_mutex_init(&user_Session[i], NULL) != 0)
            return -1;
        if (pthread_cond_init(&cond_Session[i], NULL) != 0)
            return -1;
    }
    for(j=0; j< MAX_OPEN_FILES; j++){
        openedfiles[j] = -1;
    }

    if (argc < 2) {
        printf("Please specify the pathname of the server's pipe.\n");
        return 1;
    }

    pipename = argv[1];
    
    printf("Starting TecnicoFS server with pipe called %s\n", pipename);

    if (tfs_init() == -1){
        return -1;
    }
    
    if (pthread_mutex_init(&list_lock, NULL) != 0)
            return -1;

    unlink(pipename);
    if (mkfifo(pipename, 0666) == -1){
        return -1;
    }
    
    serverpipe = open(pipename, O_RDONLY);
    if(serverpipe == -1){
        return -1;
    } 

    int op;
    for(;;){
        ret=read(serverpipe, &op, sizeof(int));
        if (ret == 0){
            close(serverpipe);
            serverpipe = open(pipename, O_RDONLY);
            if(serverpipe == -1){
                return -1;
            } 
            continue;
        } 
        if (ret == -1)
            return -1;
        char clientpipe[MAXPIPENAME];
        int id;
        char *ret_value;
        int flags;
        int fhandle;
        size_t len;
        char *buffer;
        switch (op)
        {
        case TFS_OP_CODE_MOUNT:
            ret=read(serverpipe, clientpipe, MAXPIPENAME);
            if (ret == -1){
                return -1;
            }
            tx = open(clientpipe, O_WRONLY);
            pthread_mutex_lock(&list_lock);
            for(i=0; i<S && list[i].tx != -1;i++){}
            if(tx == -1 || i >= S){
                return -1;
            }            
            list[i].tx = tx;
            list[i].id = i;
            list[i].op_on_the_way=0;
            pthread_mutex_unlock(&list_lock);
            ret_value = (char*) malloc(sizeof(int));
            memset(ret_value, i, 1);
            ret=write(tx, ret_value, sizeof(int));
            free(ret_value);
            if (ret == -1)
                return -1;
            break;
        case TFS_OP_CODE_UNMOUNT:
            ret = read(serverpipe, &id, sizeof(int));
            if (ret == -1)
                return -1;
            pthread_mutex_lock(&user_Session[id]);
            while (list[id].op_on_the_way!=0)
                pthread_cond_wait(&cond_Session[id], &user_Session[id]);
            list[id].op=op;
            list[id].id=id;
            list[id].op_on_the_way++;
            pthread_create(&t[i], NULL, worker, &id);
            pthread_cond_signal(&cond_Session[id]);
            pthread_join(t[id], NULL);
            break;
        case TFS_OP_CODE_OPEN:
            ret = read(serverpipe, &id, sizeof(int));
            if (ret == -1)
                return -1;
            pthread_mutex_lock(&user_Session[id]);
            buffer = (char *) malloc(sizeof(char)*MAXPIPENAME);
            ret = read(serverpipe, buffer, sizeof(char)*MAXPIPENAME);
            if (ret == -1)
                return -1;
            ret = read(serverpipe, &flags, sizeof(int));
            if (ret == -1)
                return -1;
            while (list[id].op_on_the_way!=0)
                pthread_cond_wait(&cond_Session[id], &user_Session[id]);
            list[id].op=op;
            strcpy(list[id].name, buffer);
            list[id].flags=flags;
            list[id].op_on_the_way++;
            free(buffer);
            pthread_create(&t[id], NULL, worker, &id);
            pthread_cond_signal(&cond_Session[id]);
            break;
        case TFS_OP_CODE_CLOSE:
            ret = read(serverpipe, &id, sizeof(int));
            if (ret == -1)
                return -1;
            pthread_mutex_lock(&user_Session[id]);
            ret = read(serverpipe, &fhandle, sizeof(int));
            if (ret == -1)
                return -1;
            while (list[id].op_on_the_way!=0)
                pthread_cond_wait(&cond_Session[id], &user_Session[id]);
            list[id].op=op;
            list[id].id=id;
            list[id].fhandle=fhandle;
            list[id].op_on_the_way++;
            pthread_create(&t[id], NULL, worker, &id);
            pthread_cond_signal(&cond_Session[id]);
            break;
        case TFS_OP_CODE_WRITE:
            ret = read(serverpipe, &id, sizeof(int));
            if (ret == -1)
                return -1;
            pthread_mutex_lock(&user_Session[id]);
            ret = read(serverpipe, &fhandle, sizeof(int));
            if (ret == -1)
                return -1;
            ret = read(serverpipe, &len, sizeof(size_t));
            if (ret == -1)
                return -1;
            while (list[id].op_on_the_way!=0)
                pthread_cond_wait(&cond_Session[id], &user_Session[id]);
            list[id].buffer=(char *) malloc(len*sizeof(char));
            ret = read(serverpipe, list[id].buffer, len*sizeof(char));
            if (ret == -1)
                return -1;
            list[id].op=op;
            list[id].id=id;
            list[id].fhandle=fhandle;
            list[id].len=(ssize_t)len;
            list[id].op_on_the_way++;
            pthread_create(&t[id], NULL, worker, &id);
            pthread_cond_signal(&cond_Session[id]);
            break;
        case TFS_OP_CODE_READ:
            ret = read(serverpipe, &id, sizeof(int));
            if (ret == -1)
                return -1;
            pthread_mutex_lock(&user_Session[id]);
            ret = read(serverpipe, &fhandle, sizeof(int));
            if (ret == -1)
                return -1;
            ret = read(serverpipe, &len, sizeof(size_t));
            if (ret == -1)
                return -1;
            while (list[id].op_on_the_way!=0)
                pthread_cond_wait(&cond_Session[id], &user_Session[id]);
            list[id].op=op;
            list[id].id=id;
            list[id].fhandle=fhandle;
            list[id].len=(ssize_t)len;
            list[id].op_on_the_way++;
            pthread_create(&t[id], NULL, worker, &id);
            pthread_cond_signal(&cond_Session[id]);
            break;
        case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
            ret = read(serverpipe, &id, sizeof(int));
            if (ret == -1){
                return -1;
            }
            pthread_mutex_lock(&user_Session[id]);
            while (list[id].op_on_the_way!=0)
                pthread_cond_wait(&cond_Session[id], &user_Session[id]);
            strcpy(list[id].name, pipename);
            list[id].op=op;
            list[id].id=id;
            list[id].op_on_the_way++;
            pthread_create(&t[id], NULL, worker, &id);
            pthread_cond_signal(&cond_Session[id]);
            return EXIT_SUCCESS;
        default:
            break;
        }
        if (op!=TFS_OP_CODE_MOUNT)
            pthread_join(t[id], NULL);
        
    }  
    return 0;
}


void *worker(void *args){
    int id;
    char *ret_value;
    char *buffer;
    int fhandle;
    int value;
    ssize_t ret;
    id = *((int*)args);
    while (list[id].op_on_the_way==0)
        pthread_cond_wait(&cond_Session[id], &user_Session[id]);
    pthread_cond_signal(&cond_Session[id]);
    switch (list[id].op)
    {
    case TFS_OP_CODE_UNMOUNT:
        pthread_mutex_lock(&list_lock);
        close(list[id].tx);
        list[id].tx = -1;
        list[id].op_on_the_way--;
        pthread_mutex_unlock(&list_lock);
        pthread_mutex_unlock(&user_Session[id]);
        break;
    case TFS_OP_CODE_OPEN:
        fhandle = tfs_open(list[id].name, list[id].flags);
        if (fhandle==-1)
            exit(1);
        ret_value =  (char *) malloc(sizeof(int));
        memset(ret_value, fhandle, 1);
        ret=write(list[id].tx, ret_value, sizeof(int));
        free(ret_value);
        if (ret == -1)
            exit(1);
        list[id].op_on_the_way--;
        pthread_mutex_unlock(&user_Session[id]);
        break;
    case TFS_OP_CODE_CLOSE:
        value =  tfs_close(list[id].fhandle);
        if (value==-1)
            exit(1);
        ret_value =  (char *) malloc(sizeof(int));
        memset(ret_value, value, 1);
        ret=write(list[id].tx, ret_value, sizeof(int));
        free(ret_value);
        if (ret == -1)
            exit(1);
        list[id].op_on_the_way--;
        pthread_mutex_unlock(&user_Session[id]);
        break;
    case TFS_OP_CODE_WRITE:
        ret = tfs_write(list[id].fhandle, list[id].buffer, (size_t) list[id].len);
        free(list[id].buffer);
        if(ret == -1){
            exit(1);
        }
        ret_value = (char *) malloc(sizeof(int));
        memset(ret_value, (int) ret, 1);
        ret = write(list[id].tx, ret_value, sizeof(int));
        free(ret_value);
        if (ret == -1)
            exit(1);
        list[id].op_on_the_way--;
        pthread_mutex_unlock(&user_Session[id]);
        break;
    case TFS_OP_CODE_READ:
        buffer=(char*) malloc(sizeof(char)*(size_t)list[id].len);
        ret = tfs_read(list[id].fhandle, buffer, (size_t) list[id].len);
        if(ret==-1){
            exit(1);
        }
        ret_value = (char *) malloc(sizeof(int)+(size_t)ret*sizeof(char));
        memset(ret_value, (int) ret, 1);
        memcpy(ret_value+sizeof(int), buffer, (size_t)ret);
        ret = write(list[id].tx, ret_value, sizeof(int)+(size_t)ret*sizeof(char));
        free(buffer);
        free(ret_value);
        if (ret == -1)
            exit(1);
        list[id].op_on_the_way--;
        pthread_mutex_unlock(&user_Session[id]);
        break;
    case TFS_OP_CODE_SHUTDOWN_AFTER_ALL_CLOSED:
        value = tfs_destroy_after_all_closed();
        
        ret_value = (char *) malloc(sizeof(int));
        memset(ret_value, value, 1);
        ret = write(list[id].tx, ret_value, sizeof(int));
        free(ret_value);
        if (ret == -1)
            exit(1);
        if(value ==-1)
            exit(1);
        for (int i=0; i<S; i++){
            if (pthread_cond_destroy(&cond_Session[i]) != 0) 
                exit(1);
            if (pthread_mutex_destroy(&user_Session[i]) != 0) 
                exit(1);
        }
        if (pthread_mutex_destroy(&list_lock) != 0) 
                exit(1); 
        close(serverpipe);
        unlink(pipename);
    default:
        break;
    }
    return NULL;
}
