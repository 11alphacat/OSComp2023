#ifndef __FS_STAT_H__
#define __FS_STAT_H__

// since mkfs will use kernel header file, add this condition preprocess
#ifndef USER
#include "common.h"
#endif

#define TFILE 2 // for xv6

// type
/*
        note: below are NOT USED any more !
    */
// #define T_DIR 1    // Directory
// #define T_FILE 2   // File
// #define T_DEVICE 3 // Device

// now use these
#define S_IFMT (0xf0)
#define S_IFIFO (0xA0)                       // 1010_0000
#define S_IFREG (0x80)                       // 1000_0000
#define S_IFBLK (0x60)                       // 0110_0000
#define S_IFDIR (0x40)                       // 0100_0000
#define S_IFCHR (0x20)                       // 0010_0000
#define S_ISREG(t) (((t)&S_IFMT) == S_IFREG) // ip->i_type
#define S_ISDIR(t) (((t)&S_IFMT) == S_IFDIR)
#define S_ISCHR(t) (((t)&S_IFMT) == S_IFCHR)
#define S_ISBLK(t) (((t)&S_IFMT) == S_IFBLK)
#define S_ISFIFO(t) (((t)&S_IFMT) == S_IFIFO)

// #define S_IFSOCK 0140000
// #define S_IFLNK	 0120000

// #define S_ISLNK(m)	(((m) & S_IFMT) == S_IFLNK)
// #define S_ISFIFO(m)	(((m) & S_IFMT) == S_IFIFO)
// #define S_ISSOCK(m)	(((m) & S_IFMT) == S_IFSOCK)

// device
#define MAJOR(rdev) ((rdev >> 8) & 0xff)
#define MINOR(rdev) (rdev & 0xff)
#define mkrdev(ma, mi) ((uint)(((ma)&0xff) << 8 | ((mi)&0xff)))
#define CONSOLE 1 // 终端的主设备号

typedef unsigned long int dev_t;
typedef unsigned long int ino_t;
typedef unsigned long int nlink_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef long int off_t;
typedef long int blksize_t;
typedef long int blkcnt_t;
typedef unsigned int mode_t;
typedef long int off_t;

struct kstat {
    uint64 st_dev;
    uint64 st_ino;
    mode_t st_mode;
    uint32 st_nlink;
    uint32 st_uid;
    uint32 st_gid;
    uint64 st_rdev;
    unsigned long __pad;
    off_t st_size;
    uint32 st_blksize;
    int __pad2;
    uint64 st_blocks;
    long st_atime_sec;
    long st_atime_nsec;
    long st_mtime_sec;
    long st_mtime_nsec;
    long st_ctime_sec;
    long st_ctime_nsec;
    unsigned __unused[2];
};

#endif // __FS_STAT_H__
