#ifndef __INODE_FS_H__
#define __INODE_FS_H__

// since mkfs will use kernel header file, add this condition preprocess
#ifndef USER
#include "common.h"
#endif
#include "fs_macro.h"

struct inode;
struct buffer_head;
struct stat;

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
    uint magic;      // Must be FSMAGIC
    uint size;       // Size of file system image (blocks)
    uint nblocks;    // Number of data blocks
    uint ninodes;    // Number of inodes.
    uint nlog;       // Number of log blocks
    uint logstart;   // Block number of first log block
    uint inodestart; // Block number of first inode block
    uint bmapstart;  // Block number of first free map block
};

// On-disk inode structure
struct dinode {
    short type;              // File type
    short major;             // Major device number (T_DEVICE only)
    short minor;             // Minor device number (T_DEVICE only)
    short nlink;             // Number of links to inode in file system
    uint size;               // Size of file (bytes)
    uint addrs[NDIRECT + 1]; // Data block addresses
};

struct dirent {
    ushort inum;
    char name[DIRSIZ];
};

#ifndef USER
int dirlink(struct inode *, char *, uint);
struct inode *dirlookup(struct inode *, char *, uint *);
struct inode *ialloc(uint, short);
struct inode *idup(struct inode *);
void ilock(struct inode *);
void iput(struct inode *);
void iunlock(struct inode *);
void iunlockput(struct inode *);
void iupdate(struct inode *);
int namecmp(const char *, const char *);
struct inode *namei(char *);
struct inode *nameiparent(char *, char *);
int readi(struct inode *, int, uint64, uint, uint);
void stati(struct inode *, struct stat *);
int writei(struct inode *, int, uint64, uint, uint);
void itrunc(struct inode *);

// log layer
void initlog(int, struct superblock *);
void log_write(struct buffer_head *);
void begin_op(void);
void end_op(void);
#endif

#endif // __INODE_FS_H__
