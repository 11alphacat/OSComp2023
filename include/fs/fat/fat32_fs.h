#ifndef __FAT_FAT32_FS_H__
#define __FAT_FAT32_FS_H__

#include "common.h"
#include "fs/fat/fat32.h"
#include "atomic/sleeplock.h"
struct dirent_tmp {
    uint64 d_ino;            // 索引结点号
    int64 d_off;             // 到下一个dirent的偏移
    unsigned short d_reclen; // 当前dirent的长度
    unsigned char d_type;    // 文件类型
    char d_name[];           //文件名
};

typedef struct dirent_tmp dirent_buf;

#define MAX_FILE_NAME 100
struct fat_entry {
    struct sleeplock lock;
    struct fat_entry *parent;
    char fname[MAX_FILE_NAME];
    short nlink;
    int ref;
    uint32 cluster_start; // start num
    uint32 cluster_end;   // end num
    uint32 parent_off;    // offset in parent clusters
    uint64 cluster_cnt;   // number of clusters
    uint32 size_in_mem;   // size in RAM

    dirent_s_t dirent_cpy;
    FATFS_t *fat_obj;
};
typedef struct fat_entry fat_entry_t;

void fat32_fs_init(void);

#endif // __FAT_FAT32_FS_H__