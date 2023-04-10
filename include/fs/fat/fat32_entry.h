#ifndef __FAT32_ENTRY_H_
#define __FAT32_ENTRY_H_

#include "common.h"
#include "atomic/spinlock.h"
#include "atomic/sleeplock.h"
#include "fs/fat/fat32_fs.h"

/* File function return code (FRESULT) */
typedef enum {
    FR_OK = 0, /* (0) Succeeded */
    // FR_DISK_ERR,            /* (1) A hard error occurred in the low level disk I/O layer */
    // FR_INT_ERR,             /* (2) Assertion failed */
    // FR_NOT_READY,           /* (3) The physical drive cannot work */
    // FR_NO_FILE,             /* (4) Could not find the file */
    // FR_NO_PATH,             /* (5) Could not find the path */
    // FR_INVALID_NAME,        /* (6) The path name format is invalid */
    // FR_DENIED,              /* (7) Access denied due to prohibited access or directory full */
    // FR_EXIST,               /* (8) Access denied due to prohibited access */
    // FR_INVALID_OBJECT,      /* (9) The file/directory object is invalid */
    // FR_WRITE_PROTECTED,     /* (10) The physical drive is write protected */
    // FR_INVALID_DRIVE,       /* (11) The logical drive number is invalid */
    // FR_NOT_ENABLED,         /* (12) The volume has no work area */
    // FR_NO_FILESYSTEM,       /* (13) There is no valid FAT volume */
    // FR_MKFS_ABORTED,        /* (14) The f_mkfs() aborted due to any problem */
    // FR_TIMEOUT,             /* (15) Could not get a grant to access the volume within defined period */
    // FR_LOCKED,              /* (16) The operation is rejected according to the file sharing policy */
    // FR_NOT_ENOUGH_CORE,     /* (17) LFN working buffer could not be allocated */
    // FR_TOO_MANY_OPEN_FILES, /* (18) Number of open files > FF_FS_LOCK */
    // FR_INVALID_PARAMETER    /* (19) Given parameter is invalid */
} FRESULT;

// Oscomp
struct fat_dirent_buf{
    uint64 d_ino;            // 索引结点号
    int64 d_off;             // 到下一个dirent的偏移
    unsigned short d_reclen; // 当前dirent的长度
    unsigned char d_type;    // 文件类型
    char d_name[];           // 文件名
};


struct fat_entry {
    sleeplock_t lock;
    struct fat_entry *parent;
    char fname[PATH_LONG_MAX];
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
    FATFS_t *fatfs_obj;
};

typedef struct fat_entry fat_entry_t;
fat_entry_t* fat32_root_entry_init(FATFS_t *);
fat_entry_t* fat32_name_fat_entry(char *);
fat_entry_t* fat32_name_fat_entry_parent(char *, char *);
fat_entry_t* fat32_fat_entry_dup(fat_entry_t*);
fat_entry_t* fat32_fat_entry_dirlookup(fat_entry_t*, char *, uint *);
fat_entry_t* fat32_fat_entry_get(uint, dirent_s_t*);

void fat32_fat_entry_trunc(fat_entry_t*);
void fat32_fat_entry_update(fat_entry_t*);
void fat32_fat_entry_lock(fat_entry_t*);
void fat32_fat_entry_unlock(fat_entry_t*);
void fat32_fat_entry_put(fat_entry_t*);
void fat32_fat_entry_unlock_put(fat_entry_t*);


int fat32_fat_entry_write(fat_entry_t*, int, uint64, uint, uint);
int fat32_fat_entry_read(fat_entry_t*, int, uint64, uint, uint);
int fat32_fat_dirlink(fat_entry_t *, char*, uint);

// void fat32_fat_entry_stat(fat_entry_t*, struct stat*);

#endif 
