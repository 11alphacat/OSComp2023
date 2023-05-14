#ifndef __FAT32_MEM_H__
#define __FAT32_MEM_H__

#include "common.h"
#include "fat32_disk.h"
#include "fat32_stack.h"
#include "fs/stat.h"

struct _inode;

// Oscomp
struct fat_dirent_buf {
    uint64 d_ino;            // 索引结点号
    int64 d_off;             // 到下一个dirent的偏移
    unsigned short d_reclen; // 当前dirent的长度
    unsigned char d_type;    // 文件类型
    char d_name[];           // 文件名
};

// fat32 super block information
struct fat32_sb_info {
    // read-only
    uint fatbase;        // FAT base sector
    uint n_fats;         // Number of FATs (1 or 2)
    uint n_sectors_fat;  // Number of sectors per FAT
    uint root_cluster_s; // Root directory base cluster (start)

    // FSINFO ~ may modify
    uint free_count;
    uint nxt_free;

    struct _inode *root_entry;
};

// fat32 inode information
struct fat32_inode_info {
    // on-disk structure
    char fname[NAME_LONG_MAX];
    uchar Attr;             // directory attribute
    uchar DIR_CrtTimeTenth; // create time
    uint16 DIR_CrtTime;     // create time, 2 bytes
    uint16 DIR_CrtDate;     // create date, 2 bytes
    uint16 DIR_LstAccDate;  // last access date, 2 bytes
    // (DIR_FstClusHI << 16) | (DIR_FstClusLO)
    uint32 cluster_start; // start num
    uint16 DIR_WrtTime;   // Last modification (write) time.
    uint16 DIR_WrtDate;   // Last modification (write) date.
    uint32 DIR_FileSize;  // file size (bytes)

    // in memory structure
    uint32 cluster_end; // end num
    uint64 cluster_cnt; // number of clusters
    uint32 parent_off;  // offset in parent clusters
};

// 0. init the root fat32 inode
struct _inode *fat32_root_inode_init(struct _superblock *);

// 1. traverse the fat32 chain
uint fat32_fat_travel(struct _inode *, uint);

// 2. return the next cluster number
uint fat32_next_cluster(uint);

// 3. allocate a new cluster
uint fat32_cluster_alloc(uint);

// 4. allocate a new fat entry
uint fat32_fat_alloc();

// 5. set the fat entry to given value
void fat32_fat_set(uint, uint);

// 6. current fat32 inode
struct _inode *fat32_name_inode(char *);

// 7. the parent of current fat32 inode
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
void fat32_inode_stati(struct _inode *, struct kstat *);

uint fat32_inode_read(struct _inode *, int, uint64, uint, uint);

// 9. write the data given the fat32 inode, offset and length
uint fat32_inode_write(struct _inode *, int, uint64, uint, uint);

// 10. dup a existed fat32 inode
struct _inode *fat32_inode_dup(struct _inode *);

// 11. find a existed or new fat32 inode
struct _inode *fat32_inode_get(uint, uint, char *, uint);

// 12. lock the fat32 inode
void fat32_inode_lock(struct _inode *);

// 13. unlock the fat32 inode
void fat32_inode_unlock(struct _inode *);

// 14. put the fat32 inode
void fat32_inode_put(struct _inode *);

// 15. unlock and put the fat32 inode
void fat32_inode_unlock_put(struct _inode *);

// 16. truncate the fat32 inode
void fat32_inode_trunc(struct _inode *);

// 17. update the fat32 inode in the disk
void fat32_inode_update(struct _inode *);

// 18. cat the Name1, Name2 and Name3 of dirent_l
int fat32_filter_longname(dirent_l_t *, char *);

// 19. reverse the dirent_l to get the long name
ushort fat32_longname_popstack(Stack_t *, uchar *, char *);

// 20. the check sum of dirent_l
uchar ChkSum(uchar *);

// 21. lookup the inode given its parent inode and name
struct _inode *fat32_inode_dirlookup(struct _inode *, char *, uint *);
struct _inode *fat32_inode_get(uint, uint, char *, uint);
void fat32_inode_stati(struct _inode *, struct kstat *);
int fat32_inode_delete(struct _inode *dp, struct _inode *ip);

// 22. create the fat32 inode
struct _inode *fat32_inode_create(char *path, uchar type, short major, short minor);

// 23. allocate the fat32 inode
struct _inode *fat32_inode_alloc(struct _inode *, char *, uchar);

// 24. init the fat32 fcb (short + long)
int fat32_fcb_init(struct _inode *, const uchar *, uchar, char *);

// 25. the number of files with the same name prefix
uint fat32_find_same_name_cnt(struct _inode *, char *);

// 26. the right fcb insert offset ?
uint fat32_dir_fcb_insert_offset(struct _inode *, uchar);

// 27. is empty?
uint fat32_isdirempty(struct _inode *);

// 28. timer to string
int fat32_time_parser(uint16 *, char *, int);

// 29. date to string
int fat32_date_parser(uint16 *, char *);

// 30. delete fat32 inode
int fat32_inode_delete(struct _inode *, struct _inode *);

// 31. acquire the time now
uint16 fat32_inode_get_time(int *);

// 32. acquire the date now
uint16 fat32_inode_get_date();

// 33. zero the cluster given cluster num
void fat32_zero_cluster(uint64 c_num);

// 34. short name parser
void fat32_short_name_parser(dirent_s_t dirent_l, char *name_buf);

// 35. load inode from disk
int fat32_inode_load_from_disk(struct _inode *ip);

#endif