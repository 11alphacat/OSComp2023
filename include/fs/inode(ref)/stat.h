#ifndef __INODE_STAT_H__
#define __INODE_STAT_H__

// since mkfs will use kernel header file, add this condition preprocess
#ifndef USER
#include "common.h"
#endif

#define T_DIR 1    // Directory
#define T_FILE 2   // File
#define T_DEVICE 3 // Device

struct stat {
    int dev;     // File system's disk device
    uint ino;    // Inode number
    short type;  // Type of file
    short nlink; // Number of links to file
    uint64 size; // Size of file in bytes
};



typedef unsigned long int dev_t;
typedef unsigned long int ino_t;
typedef unsigned int mode_t;
typedef unsigned long int nlink_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef long int off_t; 
typedef long int blksize_t;
typedef long int blkcnt_t;

struct kstat {
	dev_t st_dev;
	ino_t st_ino;
	mode_t st_mode;
	nlink_t st_nlink;
	uid_t st_uid;
	gid_t st_gid;
	dev_t st_rdev;
	unsigned long __pad;
	off_t st_size;
	blksize_t st_blksize;
	int __pad2;
	blkcnt_t st_blocks;
	long st_atime_sec;
	long st_atime_nsec;
	long st_mtime_sec;
	long st_mtime_nsec;
	long st_ctime_sec;
	long st_ctime_nsec;
	unsigned __unused[2];
};


#endif // __INODE_STAT_H__
