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


#endif // __INODE_STAT_H__
