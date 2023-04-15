#ifndef __FAT32_MEM_H__
#define __FAT32_MEM_H__


#include "common.h"
#include "fat32_disk.h"

struct _inode;

// Oscomp
struct fat_dirent_buf {
    uint64 d_ino;            // 索引结点号
    int64 d_off;             // 到下一个dirent的偏移
    unsigned short d_reclen; // 当前dirent的长度
    unsigned char d_type;    // 文件类型
    char d_name[];           // 文件名
};

struct fat32_sb_info {
    // read-only
    uint fatbase;        // FAT base sector
    uint n_fats;         // Number of FATs (1 or 2)
    uint n_sectors_fat;  // Number of sectors per FAT
    uint root_cluster_s; // Root directory base cluster (start)
    uint cluster_size;       // size of a cluster
    uint sector_per_cluster; // sector of a cluster

    // FSINFO ~ may modify
    uint free_count;
    uint nxt_free;

    struct _inode *root_entry;
};

struct fat32_inode_info {
    struct _inode *parent;
    char fname[PATH_LONG_MAX];
    uint32 cluster_start;      // start num
    uint32 cluster_end;        // end num
    uint32 parent_off;         // offset in parent clusters
    uint64 cluster_cnt;        // number of clusters
    uchar DIR_CrtTimeTenth;    // create time
    // Count of tenths of a second
    // 0 <= DIR_CrtTimeTenth <= 199
    uchar Attr;                // directory attribute
    FAT_time_t DIR_CrtTime;    // create time, 2 bytes
    FAT_date_t DIR_CrtDate;    // create date, 2 bytes
    FAT_time_t DIR_LstAccDate; // last access date, 2 bytes
    FAT_time_t DIR_WrtTime;    // Last modification (write) time.
    FAT_date_t DIR_WrtDate;    // Last modification (write) date.
    uint32_t DIR_FileSize;     // file size (bytes)
};


uint32 fat32_fat_travel(struct _inode *, uint32);

struct _inode *fat32_root_inode_init(struct _superblock *);
struct _inode *fat32_name_inode(char *);
struct _inode *fat32_name_inode_parent(char *, char *);

struct _inode *fat32_inode_dup(struct _inode *);
void fat32_inode_update(struct _inode *);
void fat32_inode_trunc(struct _inode *);

void fat32_inode_lock(struct _inode *);
void fat32_inode_unlock(struct _inode *);
void fat32_inode_put(struct _inode *);
void fat32_inode_unlock_put(struct _inode *);

int fat32_filter_longname(dirent_l_t *, char *);
struct _inode *fat32_inode_dirlookup(struct _inode *, char *, uint *);
struct _inode *fat32_inode_get(uint, uint, char *, uint);

uint fat32_inode_read(struct _inode *, int, uint64, uint, uint);
uint fat32_inode_write(struct _inode *, int, uint64, uint, uint);

uint fat32_fat_alloc();
uint fat32_cluster_alloc(uint);
uint fat32_find_same_name_cnt(struct _inode *, char *);
int fat32_fcb_init(struct _inode *, uchar *, uchar, char *);

uint fat32_next_cluster(uint);
uchar ChkSum(uchar *);

#endif 