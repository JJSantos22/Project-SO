#ifndef CONFIG_H
#define CONFIG_H

/* FS root inode number */
#define ROOT_DIR_INUM (0)

#define BLOCK_SIZE (1024)
#define DATA_BLOCKS (1024)
#define INODE_TABLE_SIZE (50)
#define MAX_OPEN_FILES (20)
#define MAX_FILE_NAME (40)

#define DELAY (5000)

#define MAX_DATA_BLOCK_SIZE (10)
#define MAX_DATA_INT (BLOCK_SIZE/sizeof(int))

#endif // CONFIG_H
