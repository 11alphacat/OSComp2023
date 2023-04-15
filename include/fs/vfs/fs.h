#ifndef __VFS_FS_H__
#define __VFS_FS_H__

#include "common.h"
#include "atomic/spinlock.h"
#include "atomic/sleeplock.h"
#include "fs/stat.h"
#include "fs/fat/fat32_mem.h"

struct stat;
struct _superblock {
    struct spinlock lock;
    uint8 s_dev; // device number

    uint32 s_blocksize;       // 逻辑块的数量
    uint32 sectors_per_block; // 每个逻辑块的扇区个数
    // uint32 s_blocksize_bits;
    uint n_sectors;   // Number of sectors
    uint sector_size; // size of a sector

    struct super_operations *s_op;
    struct _inode *s_mount;
    union {
        struct fat32_sb_info fat32_sb_info;
        // struct xv6fs_sb_info xv6fs_sb;
        // void *generic_sbp;
    };
};

struct _file {
    ushort f_mode;
    uint32 f_pos;
    ushort f_flags;
    ushort f_count;

    // struct file *f_next, *f_prev;
    int f_owner; /* pid or -pgrp where SIGIO should be sent */
    struct _inode *f_inode;
    struct file_operations *f_op;
    unsigned long f_version;
};

struct file_operations {
    char *(*getcwd)(char *__user buf, size_t size);
    int (*pipe2)(int fd[2], int flags);
    int (*dup)(int fd);
    int (*dup3)(int oldfd, int newfd, int flags);
    int (*chdir)(const char *path);
    int (*openat)(int dirfd, const char *pathname, int flags, mode_t mode);
    int (*close)(int fd);
    ssize_t (*getdents64)(int fd, void *dirp, size_t count);
    ssize_t (*read)(int fd, void *buf, size_t count);
    ssize_t (*write)(int fd, const void *buf, size_t count);
    int (*linkat)(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags);
    int (*unlinkat)(int dirfd, const char *pathname, int flags);
    int (*mkdirat)(int dirfd, const char *pathname, mode_t mode);
    int (*umount2)(const char *target, int flags);
    int (*mount)(const char *source, const char *target, const char *filesystemtype, unsigned long mountflags, const void *data);
    // int (*fstat)(int fd, struct kstat *statbuf);
};

#define NAME_MAX 10
struct _dirent {
    long d_ino;
    char d_name[NAME_MAX + 1];
};

#define IMODE_READONLY 0x1
struct _inode {
    uint8 i_dev;
    uint32 i_ino;
    uint16 i_mode;
    int ref; // Reference count
    int valid;
    // Note: fat fs does not support hard link, reserve for vfs interface
    uint16 i_nlink;

    // dev_t i_rdev;
    uint32 i_size;

    long i_atime; // access time
    long i_mtime; // modify time
    long i_ctime; // create time

    // uint32 i_blksize;
    // uint32 i_blocks;
    // struct semaphore i_sem;
    struct sleeplock i_sem;

    struct inode_operations *i_op;
    struct _superblock *i_sb;
    struct _inode *i_mount;
    // struct wait_queue *i_wait;

    union {
        struct fat32_inode_info fat32_i;
        // struct xv6_inode_info xv6_i;
        // struct ext2_inode_info ext2_i;
        // void *generic_ip;
    };
};

/* File function return code (FRESULT) */
typedef enum {
    FR_OK = 0, /* (0) Succeeded */
    // FR_DISK_ERR,            /* (1) A hard error occurred in the low level disk I/O layer */
    // FR_INT_ERR,             /* (2) Assertion failed */
    // FR_NOT_READY,           /* (3) The physical drive cannot work */
    // FR_NO_FILE,             /* (4) Could not find the file */
    // FR_NO_PATH,             /* (5) Could not find the path */
    // FR_INVALID_NAME,        /* (6) The path name format is invalid */
    // FR_DENIED,              /* (7) Access denied due to prohibited access or directory full */
    // FR_EXIST,               /* (8) Access denied due to prohibited access */
    // FR_INVALID_OBJECT,      /* (9) The file/directory object is invalid */
    // FR_WRITE_PROTECTED,     /* (10) The physical drive is write protected */
    // FR_INVALID_DRIVE,       /* (11) The logical drive number is invalid */
    // FR_NOT_ENABLED,         /* (12) The volume has no work area */
    // FR_NO_FILESYSTEM,       /* (13) There is no valid FAT volume */
    // FR_MKFS_ABORTED,        /* (14) The f_mkfs() aborted due to any problem */
    // FR_TIMEOUT,             /* (15) Could not get a grant to access the volume within defined period */
    // FR_LOCKED,              /* (16) The operation is rejected according to the file sharing policy */
    // FR_NOT_ENOUGH_CORE,     /* (17) LFN working buffer could not be allocated */
    // FR_TOO_MANY_OPEN_FILES, /* (18) Number of open files > FF_FS_LOCK */
    // FR_INVALID_PARAMETER    /* (19) Given parameter is invalid */
} FRESULT;

#endif // __VFS_FS_H__