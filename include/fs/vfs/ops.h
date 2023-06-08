#ifndef __VFS_OPS_H__
#define __VFS_OPS_H__

#include "fs.h"

// file layer
struct file *filealloc(fs_t);
void generic_fileclose(struct file *);
extern const struct file_operations *(*get_fileops[])(void);

// pathname layer
struct inode *namei(char *path);
struct inode *namei_parent(char *path, char *name);

// inode layer
<<<<<<< HEAD
struct inode *find_inode(char *path, int dirfd, char *name);
extern const struct inode_operations *(*get_inodeops[])(void);

#endif
=======
extern const struct inode_operations *(*get_inodeops[])(void);
>>>>>>> recover 2 detached commits
