#ifndef __VFS_FS_H__
#define __VFS_FS_H__

#include "common.h"
#include "atomic/spinlock.h"
#include "atomic/sleeplock.h"
#include "fs/stat.h"
#include "fs/fat/fat_info.h"

struct superblock {
    struct spinlock lock;
    uint8 s_dev;
    uint32 s_blocksize;
    uint32 s_blocksize_bits;

    //
    uint n_sectors;     // Number of sectors
    uint sector_size;        // size of a sector

    struct super_operations *s_op;
    struct inode *s_mount;
    union {
        struct fat32_sb_info fat32_sb;
        // struct xv6fs_sb_info xv6fs_sb;
        void *generic_sbp;
    };
};

struct file {
    ushort f_mode;
    uint32 f_pos;
    ushort f_flags;
    ushort f_count;

    // struct file *f_next, *f_prev;
    int f_owner; /* pid or -pgrp where SIGIO should be sent */
    struct inode *f_inode;
    struct file_operations *f_op;
    unsigned long f_version;
};

struct file_operations {
    char* (*getcwd) (char *__user buf,size_t size);
    int (*pipe2) (int fd[2], int flags);	
    int (*dup) (int fd);
    int (*dup3) (int oldfd, int newfd, int flags);
    int (*chdir) (const char* path);
    int (*openat) (int dirfd, const char *pathname, int flags, mode_t mode);
    int (*close) (int fd);
    ssize_t (*getdents64) (int fd, void *dirp, size_t count);
    ssize_t (*read) (int fd, void *buf, size_t count);
    ssize_t (*write) (int fd, const void *buf, size_t count);
    int (*linkat) (int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags);
    int (*unlinkat) (int dirfd, const char *pathname, int flags);
    int (*mkdirat) (int dirfd, const char *pathname, mode_t mode);
    int (*umount2) (const char *target, int flags);
    int (*mount) (const char *source, const char *target, const char *filesystemtype, unsigned 			long 	mountflags, const void *data);
    int (*fstat) (int fd, struct kstat *statbuf);
};

#define NAME_MAX 10
struct dirent {
    long d_ino;
	char		d_name[NAME_MAX+1];
};

struct inode {
    uint8 i_dev;
    uint32 i_ino;
    uint16 i_mode;
    int ref;               // Reference count
    int valid;
    uint16 i_nlink;

    // dev_t i_rdev;
    uint32 i_size;

    long i_atime;
    long i_mtime;
    long i_ctime;

    uint32 i_blksize;
    uint32 i_blocks;
    // struct semaphore i_sem;
    struct sleeplock i_sem;

    struct inode_operations *i_op;
    struct super_block *i_sb;
    struct inode *i_mount;
    // struct wait_queue *i_wait;

    union {
        struct fat32_inode_info fat32_i;
        // struct xv6_inode_info xv6_i;
        // struct ext2_inode_info ext2_i;
        void *generic_ip;
    } u;
};

#endif // __VFS_FS_H__