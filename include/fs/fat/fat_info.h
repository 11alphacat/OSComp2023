#ifndef __FAT_INFO_H__
#define __FAT_INFO_H__

#include "common.h"
#include "fat32_entry.h"

struct fat32_sb_info {
    uint n_fats;        // Number of FATs (1 or 2)
    uint n_sectors_fat; // Number of sectors per FAT
    uint rootbase;           // Root directory base cluster
    uint fatbase;            // FAT base sector
    uint cluster_size;       // size of a cluster
    uint sector_per_cluster; // sector of a cluster

    uint free_count;
    uint nxt_free;
    struct fat_entry *root_entry;
};

struct fat32_inode_info {
    struct fat_entry *parent;
    uint32 cluster_start; // start num
    uint32 cluster_end;   // end num
    uint32 parent_off;    // offset in parent clusters
    uint64 cluster_cnt;   // number of clusters
    uchar DIR_CrtTimeTenth; // create time
    uchar Attr;             // directory attribute
    FAT_time_t DIR_CrtTime;    // create time, 2 bytes
    FAT_date_t DIR_CrtDate;    // create date, 2 bytes
    FAT_time_t DIR_LstAccDate; // last access date, 2 bytes
    FAT_time_t DIR_WrtTime;    // Last modification (write) time.
    FAT_date_t DIR_WrtDate;    // Last modification (write) date.
    uint32_t DIR_FileSize; // file size (bytes)
};


#endif // __FAT_INFO_H__