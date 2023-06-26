#ifndef __VFS_FS_H__
#define __VFS_FS_H__

#include "common.h"
#include "param.h"
#include "atomic/spinlock.h"
#include "atomic/semaphore.h"
#include "fs/stat.h"
#include "fs/fcntl.h"
#include "fs/fat/fat32_mem.h"
#include "lib/hash.h"
#include "lib/radix-tree.h"
#include "lib/list.h"

struct kstat;
extern struct ftable _ftable;

union file_type {
    struct pipe *f_pipe;   // FD_PIPE
    struct inode *f_inode; // FDINODE and FD_DEVICE
};

typedef enum {
    FAT32 = 1,
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

    struct spinlock dirty_lock; // used to protect dirty list
    struct list_head s_dirty;   /* dirty inodes */
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
    short f_major;

    int f_owner; /* pid or -pgrp where SIGIO should be sent */
    union file_type f_tp;
    const struct file_operations *f_op; // don't use pointer (bug maybe)!!!!
    // unsigned long f_version;
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
    uint8 i_dev;  // note: 未在磁盘中存储
    uint32 i_ino; // 对任意给定的文件系统的唯一编号标识：由具体文件系统解释
    // uint16 i_mode; // 访问权限和所有权
    uint16 i_mode; // 文件类型 + ..? + 访问权限  (4 + 3 + 9) : obey Linux
    int ref;       // Reference count
    int valid;
    // Note: fat fs does not support hard link, reserve for vfs interface
    uint16 i_nlink;
    uint i_uid;
    uint i_gid;
    uint16 i_rdev; // major、minor, 8 + 8
    uint32 i_size;
    // uint16 i_type;       // we do no use it anymore

    long i_atime;     // access time
    long i_mtime;     // modify time
    long i_ctime;     // create time
    uint64 i_blksize; // bytes of one block
    uint64 i_blocks;  // numbers of blocks

    struct semaphore i_sem; /* binary semaphore */

    const struct inode_operations *i_op;
    struct _superblock *i_sb;
    struct inode *i_mount;
    // struct wait_queue *i_wait;
    struct inode *parent;

    fs_t fs_type;

    // O(1) to get inode information
    struct hash_table *i_hash;

    // speed up dirlookup
    int off_hint;

    struct spinlock i_lock;          // protecting other fields
    uint64 i_writeback;              // writing back ?
    struct list_head dirty_list;     // link with superblock s_dirty
    struct address_space *i_mapping; // used for page cache
    spinlock_t tree_lock;            /* and lock protecting radix tree */

    struct list_head list; // to speed up inode_get

    int dirty_in_parent; // need to update ??

    union {
        struct fat32_inode_info fat32_i;
        // struct xv6inode_info xv6_i;
        // struct ext2inode_info ext2_i;
        // void *generic_ip;
    };
};

#define PAGECACHE_TAG_DIRTY 0
#define PAGECACHE_TAG_WRITEBACK 1
struct address_space {
    struct inode *host;               /* owner: inode*/
    struct radix_tree_root page_tree; /* radix tree(root) of all pages */
    uint64 nrpages;                   /* number of total pages */
    uint64 last_index;                // 2 4 6 8 ... read head policy
    uint64 read_ahead_cnt;            // the number of read ahead
    uint64 read_ahead_end;            // the end index of read ahead
};

struct file_operations {
    // struct file *(*alloc)(void);
    struct file *(*dup)(struct file *self);
    ssize_t (*read)(struct file *self, uint64 __user dst, int n);
    ssize_t (*write)(struct file *self, uint64 __user src, int n);
    int (*fstat)(struct file *self, uint64 __user dst);
    // int (*ioctl) (struct inode *, struct file *, unsigned int cmd, unsigned long __user arg);
    long (*ioctl)(struct file *self, unsigned int cmd, unsigned long arg);
};

struct inode_operations {
    void (*iunlock_put)(struct inode *self);
    void (*iunlock)(struct inode *self);
    void (*iput)(struct inode *self);
    void (*ilock)(struct inode *self);
    void (*iupdate)(struct inode *self);
    struct inode *(*idup)(struct inode *self);
    void (*ipathquery)(struct inode *self, char *kbuf);
    ssize_t (*iread)(struct inode *self, int user_src, uint64 src, uint off, uint n);
    ssize_t (*iwrite)(struct inode *self, int user_dst, uint64 dst, uint off, uint n);

    // for directory inode
    struct inode *(*idirlookup)(struct inode *self, const char *name, uint *poff);
    int (*idempty)(struct inode *self);
    ssize_t (*igetdents)(struct inode *self, char *buf, uint32 off, size_t len);
    struct inode *(*icreate)(struct inode *self, const char *name, uint16 type, short major, short minor);
    int (*ientrycopy)(struct inode *dself, const char *name, struct inode *ip);
    int (*ientrydelete)(struct inode *dself, struct inode *ip);
};

struct linux_dirent {
    uint64 d_ino;            // 索引结点号
    int64 d_off;             // 到下一个dirent的偏移
    unsigned short d_reclen; // 当前dirent的长度
    unsigned char d_type;    // 文件类型
    char d_name[];           //文件名
};

#endif // __VFS_FS_H__