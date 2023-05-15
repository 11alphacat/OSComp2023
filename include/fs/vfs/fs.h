#ifndef __VFS_FS_H__
#define __VFS_FS_H__

#include "common.h"
#include "param.h"
#include "atomic/spinlock.h"
#include "atomic/semaphore.h"
#include "fs/stat.h"
#include "fs/fcntl.h"
#include "fs/vfs/fs.h"
#include "fs/fat/fat32_mem.h"

struct kstat;
extern struct ftable _ftable;

union file_type {
    struct pipe *f_pipe;    // FD_PIPE
    struct inode *f_inode; // FDINODE and FD_DEVICE
};

typedef enum {
    FAT32=1,
    EXT2,
} fs_t;

struct _superblock {
    struct semaphore sem; /* binary semaphore */
    uint8 s_dev;          // device number

    uint32 s_blocksize;       // 逻辑块的数量
    uint32 sectors_per_block; // 每个逻辑块的扇区个数
    uint cluster_size;        // size of a cluster

    // uint32 s_blocksize_bits;
    uint n_sectors;   // Number of sectors
    uint sector_size; // size of a sector

    struct super_operations *s_op;
    struct inode *s_mount;
    struct inode *root;

    union {
        struct fat32_sb_info fat32_sb_info;
        // struct xv6fs_sb_info xv6fs_sb;
        // void *generic_sbp;
    };
};

// abstarct everything in memory
struct file {
    type_t f_type;
    ushort f_mode;
    uint32 f_pos;
    ushort f_flags;
    ushort f_count;
    short f_major; // FD_DEVICE

    int f_owner;                        /* pid or -pgrp where SIGIO should be sent */
    union file_type f_tp;               
    const struct file_operations *f_op; // don't use pointer (bug maybe)!!!!
    unsigned long f_version;
};


struct ftable {
    struct spinlock lock;
    struct file file[NFILE];
};

// #define NAME_MAX 10
// struct _dirent {
//     long d_ino;
//     char d_name[NAME_MAX + 1];
// };


// abstract datas in disk
struct inode {
    uint8 i_dev;
    uint32 i_ino;  // 对任意给定的文件系统的唯一编号标识：由具体文件系统解释
    uint16 i_mode; // 访问权限和所有权
    int ref;       // Reference count
    int valid;
    // Note: fat fs does not support hard link, reserve for vfs interface
    uint16 i_nlink;
    uint i_uid;
    uint i_gid;
    uint64 i_rdev;
    uint32 i_size;
    uint16 i_type;

    long i_atime;     // access time
    long i_mtime;     // modify time
    long i_ctime;     // create time
    uint64 i_blksize; // bytes of one block
    uint64 i_blocks;  // numbers of blocks
    // uint32 i_blksize;
    // uint32 i_blocks;
    struct semaphore i_sem; /* binary semaphore */
    // struct sleeplock i_sem;

    const struct inode_operations *i_op;
    struct _superblock *i_sb;
    struct inode *i_mount;
    // struct wait_queue *i_wait;
    struct inode *parent;

    fs_t fs_type;
    union {
        struct fat32_inode_info fat32_i;
        // struct xv6inode_info xv6_i;
        // struct ext2inode_info ext2_i;
        // void *generic_ip;
    };

};

struct file_operations {
    struct file* (*alloc) (void);
    struct file* (*dup) (struct file*);
    ssize_t (*read) (struct file*, uint64 __user, int);
    ssize_t (*write) (struct file*, uint64 __user, int);
    int (*fstat) (struct file *, uint64 __user);
};


struct inode_operations {
    void (*iunlock_put) (struct inode* self);
    void (*iunlock) (struct inode* self);
    void (*iput) (struct inode* self);
    void (*ilock) (struct inode* self);
    void (*itrunc) (struct inode* self);
    void (*iupdate) (struct inode* self);
    struct inode* (*idup) (struct inode* self);
    int (*idir) (struct inode* self);   // is self a directory

    // for directory inode
    struct inode* (*idirlookup) (struct inode* self, const char* name, uint* poff);
    int (*idelete) (struct inode* dp, struct inode* ip);
    int (*idempty) (struct inode* dp);
    ssize_t (*igetdents) (struct inode *dp, char *buf, size_t len);
};


struct linux_dirent {
    uint64 d_ino;            // 索引结点号
    int64 d_off;             // 到下一个dirent的偏移
    unsigned short d_reclen; // 当前dirent的长度
    unsigned char d_type;    // 文件类型
    char d_name[];           //文件名
};



#endif // __VFS_FS_H__