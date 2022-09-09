#include "operations.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <unistd.h>


int tfs_init() {
    state_init();

    /* create root inode */
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }

    return 0;
}

int tfs_destroy() {
    state_destroy();
    return 0;
}

static bool valid_pathname(char const *name) {
    return name != NULL && strlen(name) > 1 && name[0] == '/';
}


int tfs_lookup(char const *name) {
    if (!valid_pathname(name)) {
        return -1;
    }

    // skip the initial '/' character
    name++;

    return find_in_dir(ROOT_DIR_INUM, name);
}

int tfs_open(char const *name, int flags) {
    int inum;
    size_t offset;

    /* Checks if the path name is valid */
    if (!valid_pathname(name)) {
        return -1;
    }

    inum = tfs_lookup(name);
    if (inum >= 0) {
        /* The file already exists */
        inode_t *inode = inode_get(inum);
        if (inode == NULL) {
            return -1;
        }
        pthread_rwlock_wrlock(&inode->lock);
        /* Trucate (if requested) */
        if (flags & TFS_O_TRUNC) {
            if (inode->i_size > 0) {
                size_t nBlocks = inode->i_size/BLOCK_SIZE;
                for(size_t i = 0; i<MAX_DATA_BLOCK_SIZE && i<nBlocks; i++){
                    if (data_block_free(inode->i_data[i]) == -1) {
                        pthread_rwlock_unlock(&inode->lock);
                        return -1;
                    }
                }
                if(nBlocks >= MAX_DATA_BLOCK_SIZE){
                    nBlocks -= MAX_DATA_BLOCK_SIZE;
                    int *i_pointer=(int *)data_block_get(inode->i_data_block);
                    if (i_pointer==NULL){
                        pthread_rwlock_unlock(&inode->lock);
                        return -1;
                    }
                    for (size_t i = 0; i<MAX_DATA_INT && i<nBlocks; i++){
                        if (data_block_free(i_pointer[i]) == -1) {
                            pthread_rwlock_unlock(&inode->lock);
                            return -1;
                        }
                    }
                    if (data_block_free(inode->i_data_block) == -1) {
                        pthread_rwlock_unlock(&inode->lock);
                        return -1;
                    }
                }
                inode->i_size = 0;
            }
        }
        /* Determine initial offset */
        if (flags & TFS_O_APPEND) {
            offset = inode->i_size;
        } else {
            offset = 0;
        }
        pthread_rwlock_unlock(&inode->lock);
    } 
    else if (flags & TFS_O_CREAT) {
        /* The file doesn't exist; the flags specify that it should be created*/
        /* Create inode */
        inum = inode_create(T_FILE);
        if (inum == -1) {
            return -1;
        }
        /* Add entry in the root directory */
        if (add_dir_entry(ROOT_DIR_INUM, inum, name + 1) == -1) {
            inode_delete(inum);
            return -1;
        }
        offset = 0;
    } else {
        return -1;
    }

    /* Finally, add entry to the open file table and
     * return the corresponding handle */
    return add_to_open_file_table(inum, offset);

    /* Note: for simplification, if file was created with TFS_O_CREAT and there
     * is an error adding an entry to the open file table, the file is not
     * opened but it remains created */
}


int tfs_close(int fhandle) { return remove_from_open_file_table(fhandle); }

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    pthread_mutex_lock(&file->lock);

    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        pthread_mutex_unlock(&file->lock);
        return -1;
    }

    pthread_rwlock_wrlock(&inode->lock);


    /* Determine how many bytes to write */
    if (to_write + file->of_offset > MAX_DATA_BLOCK_SIZE*BLOCK_SIZE+MAX_DATA_INT*BLOCK_SIZE) {
        to_write = MAX_DATA_BLOCK_SIZE*BLOCK_SIZE+MAX_DATA_INT*BLOCK_SIZE - file->of_offset;
    }

    if (to_write > 0) {
        
        size_t start=file->of_offset/((size_t)BLOCK_SIZE);
        size_t memval=to_write;
        size_t insider=file->of_offset%DATA_BLOCKS;
        size_t var;
        /* Starts writing from the first data block with free space */
        for (size_t i=start;i<(size_t)MAX_DATA_BLOCK_SIZE && memval>0;i++){
            
            if (insider==0){
                inode->i_data[i]= data_block_alloc();
            }
            void *block = data_block_get(inode->i_data[i]);
            if (block == NULL) {
                pthread_rwlock_unlock(&inode->lock);
                pthread_mutex_unlock(&file->lock);
                return -1;
            }
            var = BLOCK_SIZE - insider;
            if(var > memval){
                var = memval;
            }
            /*Perform the actual write*/
            memcpy(block + insider, buffer, var);
            buffer += var;
            insider = 0;
            memval-=var;

        }
         /*The offset associated with the file handle is*/
         /* incremented accordingly */
        file->of_offset += to_write-memval;
        if (file->of_offset > inode->i_size) {
            inode->i_size = file->of_offset;
        } 
        size_t copy_memval=memval;
        if (memval>0){
            if(inode->i_data_block==-1){
                inode->i_data_block=data_block_alloc();
            }
            int *i_pointer=(int *)data_block_get(inode->i_data_block);
            if (i_pointer==NULL){
                pthread_rwlock_unlock(&inode->lock);
                pthread_mutex_unlock(&file->lock);
                return -1;
            }
            start=file->of_offset/((size_t)BLOCK_SIZE);
            insider=file->of_offset%DATA_BLOCKS;
            /* Starts writing from the first data block with free space */
            for (size_t i=start;i<(size_t)MAX_DATA_INT && memval>0;i++){
                if (insider==0){
                    i_pointer[i]= data_block_alloc();
                }
                void *block = data_block_get(i_pointer[i]);
                if (block == NULL) {
                    pthread_rwlock_unlock(&inode->lock);
                    pthread_mutex_unlock(&file->lock);
                    return -1;
                }
                var = BLOCK_SIZE - insider;
                if(var > memval){
                    var = memval;
                }
                /*Perform the actual write*/
                memcpy(block + insider, buffer, var);
                buffer += var;
                insider = 0;
                memval-=var;
            } 
        }

         /*The offset associated with the file handle is*/
         /* incremented accordingly */
        file->of_offset += copy_memval;
        if (file->of_offset > inode->i_size) {
            inode->i_size = file->of_offset;
        } 
    }
    pthread_rwlock_unlock(&inode->lock);
    pthread_mutex_unlock(&file->lock);

    return (ssize_t)to_write;
}


ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    pthread_mutex_lock(&file->lock);

    /* From the open file table entry, we get the inode */
    inode_t *inode = inode_get(file->of_inumber);
    if (inode == NULL) {
        pthread_mutex_unlock(&file->lock);
        return -1;
    }

    pthread_rwlock_rdlock(&inode->lock);

    /* Determine how many bytes to read */
    size_t to_read = inode->i_size - file->of_offset;
    if (to_read > len) {
        to_read = len;
    }
    size_t memval=to_read;
    size_t start=file->of_offset/((size_t)BLOCK_SIZE);
    size_t insider=file->of_offset%DATA_BLOCKS;
    size_t var;
    if (to_read > 0) {
        for (size_t i=start; i<(size_t) MAX_DATA_BLOCK_SIZE && memval>0;i++){
            void *block = data_block_get(inode->i_data[i]);
            if (block == NULL) {
                pthread_rwlock_unlock(&inode->lock);
                pthread_mutex_unlock(&file->lock);
                return -1;
            }
            var = BLOCK_SIZE - insider;
            if (var>memval)
                var=memval;

            /*Perform the actual read */
            memcpy(buffer, block+insider, var);
            buffer += var;
            insider=0;
            memval-=var;
        }

         /*The offset associated with the file handle is
         incremented accordingly */
        file->of_offset += to_read-memval;
        size_t copy_memval=memval;
        if (memval>0){
            int *i_pointer=(int *)data_block_get(inode->i_data_block);
            if (i_pointer==NULL){
                pthread_rwlock_unlock(&inode->lock);
                pthread_mutex_unlock(&file->lock);
                return -1;
            }
            start=file->of_offset/((size_t)BLOCK_SIZE);
            insider=file->of_offset%DATA_BLOCKS;
            /* Starts writing from the first data block with free space */
            for (size_t i=start;i<(size_t)MAX_DATA_INT && memval>0;i++){
                void *block = data_block_get(i_pointer[i]);
                if (block == NULL) {
                    pthread_rwlock_unlock(&inode->lock);
                    pthread_mutex_unlock(&file->lock);
                    return -1;
                }
                var = BLOCK_SIZE - insider;
                if (var>memval)
                    var=memval;

                /*Perform the actual read */
                memcpy(buffer, block+insider, var);
                buffer += var;
                insider=0;
                memval-=var;
            } 
        }
        /*The offset associated with the file handle is
         incremented accordingly */
        file->of_offset += copy_memval;
    }
    pthread_rwlock_unlock(&inode->lock);
    pthread_mutex_unlock(&file->lock);

    return (ssize_t)to_read;
}

 int tfs_copy_to_external_fs(char const *source_path, char const *dest_path){
    int source=tfs_lookup(source_path);
    if (source==-1)
        return -1;
    FILE* destiny=fopen(dest_path, "w");
    if (destiny == NULL){
        return -1;
    }
    source = tfs_open(source_path,0);
    open_file_entry_t *file = get_open_file_entry(source);
    inode_t *inode = inode_get(file->of_inumber);
    char buffer[inode->i_size];
    size_t len = (size_t) tfs_read(source, buffer, inode->i_size);
    fwrite(buffer, sizeof(char), len, destiny);
    tfs_close(source);
    fclose(destiny);
    return 0;
}
