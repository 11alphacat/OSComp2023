#ifndef __FAT32_FILE_H__
#define __FAT32_FILE_H__

#include "common.h"

extern struct devsw devsw[];

void fat32_fileinit(void);
struct file *fat32_filedup(struct file *);
ssize_t fat32_fileread(struct file *, uint64, int n);
int fat32_filestat(struct file *, uint64 addr);
ssize_t fat32_filewrite(struct file *, uint64, int n);
void fat32_getcwd(char *buf);
ssize_t fat32_getdents(struct inode *dp, char *buf, size_t len);
#endif