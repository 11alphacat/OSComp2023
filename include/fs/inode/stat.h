#ifndef __FS_STAT_H__
#define __FS_STAT_H__

// since mkfs will use kernel header file, add this condition preprocess
#ifndef USER
#include "common.h"
#endif

#define T_DIR 1    // Directory
#define T_FILE 2   // File
#define T_DEVICE 3 // Device


typedef unsigned long int dev_t;
typedef unsigned long int ino_t;
typedef unsigned long int nlink_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef long int off_t;
typedef long int blksize_t;
typedef long int blkcnt_t;


#endif // __FS_STAT_H__
