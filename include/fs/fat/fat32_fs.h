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
    int valid;
    uint32 cluster_start; // start num
    uint32 cluster_end;   // end num
    uint32 parent_off;    // offset in parent clusters
    uint64 cluster_cnt;   // number of clusters

    // dirent_s_t dirent_cpy;
    uchar Attr;             // directory attribute
    uchar DIR_CrtTimeTenth; // create time
    // Count of tenths of a second
    // 0 <= DIR_CrtTimeTenth <= 199
    FAT_time_t DIR_CrtTime;    // create time, 2 bytes
    FAT_date_t DIR_CrtDate;    // create date, 2 bytes
    FAT_time_t DIR_LstAccDate; // last access date, 2 bytes
    FAT_time_t DIR_WrtTime;    // Last modification (write) time.
    FAT_date_t DIR_WrtDate;    // Last modification (write) date.

    uint32_t DIR_FileSize; // file size (bytes)
    uint32_t DIR_FstClus;  // first data cluster number
    // uchar DIR_FstClusHI[2];    // High word of
    // uchar DIR_FstClusLO[2];    // Low word of first data cluster number

    FATFS_t *fat_obj;
};
typedef struct fat_entry fat_entry_t;

FRESULT fat32_fs_init(void);
FRESULT fat32_fs_mount(int, FATFS_t *);
FRESULT fat32_boot_sector_parser(FATFS_t *, fat_bpb_t *);
FRESULT fat32_fsinfo_parser(FATFS_t *, fsinfo_t *);

#endif // __FAT_FAT32_FS_H__