#ifndef __INODE_FILE_H__
#define __INODE_FILE_H__

#include "common.h"
#include "fs/inode/fs_macro.h"
#include "atomic/sleeplock.h"
#include "proc/semaphore.h"

struct pipe;
struct inode;

struct file {
    enum { FD_NONE,
           FD_PIPE,
           FD_INODE,
           FD_DEVICE } type;
    int ref; // reference count
    char readable;
    char writable;
    struct pipe *pipe; // FD_PIPE
    struct inode *ip;  // FD_INODE and FD_DEVICE
    uint off;          // FD_INODE
    short major;       // FD_DEVICE
};

// in-memory copy of an inode
struct inode {
    uint dev;              // Device number
    uint inum;             // Inode number
    int ref;               // Reference count
    struct sleeplock lock; // protects everything below here
    // struct semaphore sem_i;

    int valid;             // inode has been read from disk?

    short type; // copy of disk inode
    short major;
    short minor;
    short nlink;
    uint size;
    uint addrs[NDIRECT + 1];
};

// map major device number to device functions.
struct devsw {
    int (*read)(int, uint64, int);
    int (*write)(int, uint64, int);
};

extern struct devsw devsw[];

struct file *filealloc(void);
void fileclose(struct file *);
struct file *filedup(struct file *);
int fileread(struct file *, uint64, int n);
int filestat(struct file *, uint64 addr);
int filewrite(struct file *, uint64, int n);

#endif // __INODE_FILE_H__
