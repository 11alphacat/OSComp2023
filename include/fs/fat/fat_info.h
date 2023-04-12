#ifndef __FAT_INFO_H__
#define __FAT_INFO_H__

#include "common.h"
#include "fat32_entry.h"

struct _inode;
struct fat32_sb_info {
    // read-only
    uint fatbase;        // FAT base sector
    uint n_fats;         // Number of FATs (1 or 2)
    uint n_sectors_fat;  // Number of sectors per FAT
    uint root_cluster_s; // Root directory base cluster
    // uint root_sector_s;      // Root directory base sector
    uint cluster_size;       // size of a cluster
    uint sector_per_cluster; // sector of a cluster
    // uint rootdir_entries;

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
    uchar Attr;                // directory attribute
    FAT_time_t DIR_CrtTime;    // create time, 2 bytes
    FAT_date_t DIR_CrtDate;    // create date, 2 bytes
    FAT_time_t DIR_LstAccDate; // last access date, 2 bytes
    FAT_time_t DIR_WrtTime;    // Last modification (write) time.
    FAT_date_t DIR_WrtDate;    // Last modification (write) date.
    uint32_t DIR_FileSize;     // file size (bytes)
};

#endif // __FAT_INFO_H__