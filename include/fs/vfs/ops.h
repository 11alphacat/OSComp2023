#include "fs.h"

// file layer
struct file *filealloc(void);
void generic_fileclose(struct file *);
extern const struct file_operations* (*get_fileops[])(void);


// inode layer
struct inode* find_inode(char *path, int dirfd, char *name);
struct inode* namei(char *path);
struct inode* namei_parent(char *path, char *name);
extern const struct inode_operations* (*get_inodeops[])(void);