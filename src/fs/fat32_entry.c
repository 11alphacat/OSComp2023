#include "common.h"
#include "memory/list_alloc.h"
#include "fs/bio.h"
#include "atomic/spinlock.h"
#include "kernel/trap.h"
#include "fs/fat/fat32_fs.h"
#include "debug.h"
#include "param.h"
#include "fs/fat/fat32_entry.h"
#include "kernel/proc.h"
#include "fs/stat.h"
#include "fs/vfs/fs.h"
#include "fs/fat/fat32_stack.h"

extern struct file_operations fat32_fop;
extern struct inode_operations fat32_iop;

extern FATFS_t global_fatfs;

// maximum file entry in memory
#define NENTRY 10
struct fat_entry_table_t {
    spinlock_t *lock;
    fat_entry_t fat_table[NENTRY];
} fat_entry_table;

void fat32_fat_entry_init() {
    fat_entry_t *entry;
    for (entry = fat_entry_table.fat_table; entry < &fat_entry_table.fat_table[NENTRY]; entry++) {
        memset(entry, 0, sizeof(fat_entry_t));
        initsleeplock(&entry->lock, "fat_entry");
    }
}

// num is a logical cluster number(within a file)
// num == 0 ~ return count of clusters, set the inode->fat32_i.cluster_end at the same time
// num != 0 ~ return the num'th physical cluster num
uint32 fat32_fat_travel(struct _inode *ip, uint32 num) {
    struct buffer_head *bp;

    int flags = 0;
    if (num == 0) {
        flags = 1;
    }

    FAT_entry_t iter_n = ip->fat32_i.cluster_start;
    uint32 FAT_sec_no;
    int cnt = 0;
    int prev = 0;
    while (!ISEOF(iter_n) && (num > ++cnt || flags == 1)) {
        FAT_sec_no = ThisFATEntSecNum(iter_n);
        bp = bread(ROOTDEV, FAT_sec_no);
        FAT_entry_t fat_next = FAT32NextCluster(bp->data, iter_n);
        brelse(bp);

        prev = iter_n;
        iter_n = fat_next;
    }

    if (flags == 1) {
        ip->fat32_i.cluster_end = prev;
        return cnt;
    } else {
        return iter_n;
    }
}

struct _inode *fat32_root_entry_init(struct _superblock *sb) {
    // root inode initialization
    struct _inode *root_ip = (struct _inode *)kalloc();
    initsleeplock(&root_ip->i_sem, "fat_root_inode");
    root_ip->i_dev = sb->s_dev;
    root_ip->i_mode |= IMODE_READONLY;
    // set root inode num to 0
    root_ip->i_ino = 0;
    root_ip->ref = 0;
    root_ip->valid = 1;
    // file size
    root_ip->i_mount = root_ip;
    root_ip->i_sb = sb;
    root_ip->i_op = TODO();
    root_ip->i_nlink = 1;

    // the parent of the root is itself
    root_ip->fat32_i.parent = root_ip;
    root_ip->fat32_i.cluster_start = 2;
    root_ip->fat32_i.parent_off = 0;
    root_ip->fat32_i.fname[0] = '/';
    root_ip->fat32_i.fname[1] = '\0';

    // root inode doesn't have these fields, so set these to 0
    root_ip->i_atime = 0;
    root_ip->i_mtime = 0;
    root_ip->i_ctime = 0;

    root_ip->fat32_i.cluster_cnt = fat32_fat_travel(root_ip, 0);
    root_ip->i_size = root_ip->fat32_i.cluster_cnt * root_ip->i_sb->s_blocksize;
    root_ip->i_sb = sb;
    root_ip->fat32_i.DIR_FileSize = 0;
    DIR_SET(root_ip->fat32_i.Attr);

    ASSERT(root_ip->fat32_i.cluster_start == 2);

    return root_ip;
}

// Examples:
//   skepelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skepelem("///a//bb", name) = "bb", setting name = "a"
//   skepelem("a", name) = "", setting name = "a"
//   skepelem("", name) = skepelem("////", name) = 0
static char *skepelem(char *path, char *name) {
    char *s;
    int len;

    while (*path == '/')
        path++;
    if (*path == 0)
        return 0;
    s = path;
    while (*path != '/' && *path != 0)
        path++;
    len = path - s;
    if (len >= PATH_LONG_MAX)
        memmove(name, s, PATH_LONG_MAX);
    else {
        memmove(name, s, len);
        name[len] = 0;
    }
    while (*path == '/')
        path++;
    return path;
}

static fat_entry_t *fat32_fat_entry_namex(char *path, int nameeparent, char *name) {
    fat_entry_t *fat_ep=NULL, *next=NULL;

    if (*path == '/')
        fat_ep = fat32_fat_entry_dup(&global_fatfs.root_entry);
    else
        fat_ep = fat32_fat_entry_dup(myproc()->fat_cwd);

    while ((path = skepelem(path, name)) != 0) {
        fat32_fat_entry_lock(fat_ep);
        if (!DIR_BOOL(fat_ep->Attr)) {
            fat32_fat_entry_unlock_put(fat_ep);
            return 0;
        }
        if (nameeparent && *path == '\0') {
            // Stop one level early.
            fat32_fat_entry_lock(fat_ep);
            return fat_ep;
        }
        if ((next = fat32_fat_entry_dirlookup(fat_ep, name, 0)) == 0) {
            fat32_fat_entry_put(fat_ep);
            return 0;
        }
        fat32_fat_entry_put(fat_ep);
        fat_ep = next;
    }
    if (nameeparent) {
        fat32_fat_entry_put(fat_ep);
        return 0;
    }
    return fat_ep;
}

fat_entry_t *fat32_name_fat_entry(char *path) {
    char name[PATH_LONG_MAX]; // 260
    return fat32_fat_entry_namex(path, 0, name);
}

fat_entry_t *fat32_name_fat_entry_parent(char *path, char *name) {
    return fat32_fat_entry_namex(path, 1, name);
}

fat_entry_t *fat32_fat_entry_dup(fat_entry_t *fat_ep) {
    acquire(&fat_entry_table.lock);
    fat_ep->ref+fat_ep->ref++;
    release(&fat_entry_table.lock);
    return fat_ep;
}

void fat32_fat_entry_update(fat_entry_t *fat_ep) {
    struct buffer_head *bp;
    bp = bread(fat_ep->fatfs_obj->dev, FATNUM_TO_SECTOR(fat_ep->fat_num));
    dirent_s_t *dirent_s_tmp = (dirent_s_t *)bp->data + FATNUM_TO_OFFSET(fat_ep->fat_num);

    dirent_s_tmp->DIR_Attr = fat_ep->Attr;
    memmove((void *)&dirent_s_tmp->DIR_LstAccDate, (void *)&fat_ep->DIR_LstAccDate, sizeof(fat_ep->DIR_LstAccDate));
    memmove((void *)&dirent_s_tmp->DIR_WrtDate, (void *)&fat_ep->DIR_WrtDate, sizeof(fat_ep->DIR_WrtDate));
    memmove((void *)&dirent_s_tmp->DIR_WrtTime, (void *)&fat_ep->DIR_WrtTime, sizeof(fat_ep->DIR_WrtTime));
    dirent_s_tmp->DIR_FstClusHI = DIR_FIRST_HIGH(fat_ep->cluster_start);
    dirent_s_tmp->DIR_FstClusLO = DIR_FIRST_LOW(fat_ep->cluster_end);
    dirent_s_tmp->DIR_FileSize = fat_ep->DIR_FileSize;

    bwrite(bp);
    brelse(bp);
}

void fat32_fat_entry_trunc(fat_entry_t *fat_ep) {
    struct buffer_head *bp;

    FAT_term_t iter_n = fat_ep->cluster_start;
    uint32 FAT_s_n;
    uint32 FAT_s_offset;
    FAT_term_t end = 0;

    while (!ISEOF(iter_n)) {
        FAT_s_n = ThisFATSecNum(iter_n);
        FAT_s_offset = ThisFATEntOffset(iter_n);
        bp = bread(fat_ep->fatfs_obj->dev, FAT_s_n);
        FAT_term_t fat_next = FAT32ClusEntryVal(bp->data, iter_n);

        SetFAT32ClusEntryVal(bp->data, iter_n, EOC_MASK);

        bwrite(bp);
        brelse(bp);
        iter_n = fat_next;
    }
    fat_ep->cluster_start = -1;
    fat_ep->cluster_end = -1;
    fat_ep->parent_off = -1;
    fat_ep->cluster_cnt = -1;
    fat_ep->DIR_FileSize = 0;
    fat32_fat_entry_update(fat_ep);
}

// 获取fat_entry的锁
void fat32_fat_entry_lock(fat_entry_t *fat_ep) {
    struct buffer_head *bp;
    if (fat_ep == 0 || fat_ep->ref < 1)
        panic("ilock");
    acquiresleep(&fat_ep->lock);
    if (fat_ep->valid == 0) {
        bp = bread(fat_ep->fatfs_obj->dev, FATNUM_TO_SECTOR(fat_ep->fat_num));
        dirent_s_t *dirent_s_tmp = (dirent_s_t *)bp->data + FATNUM_TO_OFFSET(fat_ep->fat_num);

        fat_ep->Attr = dirent_s_tmp->DIR_Attr;
        fat_ep->cluster_start = DIR_FIRST_CLUS(dirent_s_tmp->DIR_FstClusHI, dirent_s_tmp->DIR_FstClusLO);
        fat_ep->cluster_cnt = fat32_fat_travel(fat_ep->cluster_start, &(fat_ep->cluster_end));
        fat_ep->DIR_CrtTimeTenth = dirent_s_tmp->DIR_CrtTimeTenth;

        memmove((void *)&fat_ep->DIR_CrtTime, (void *)&dirent_s_tmp->DIR_CrtTime, sizeof(dirent_s_tmp->DIR_CrtTime));
        memmove((void *)&fat_ep->DIR_CrtDate, (void *)&dirent_s_tmp->DIR_CrtDate, sizeof(dirent_s_tmp->DIR_CrtDate));
        memmove((void *)&fat_ep->DIR_LstAccDate, (void *)&dirent_s_tmp->DIR_LstAccDate, sizeof(dirent_s_tmp->DIR_LstAccDate));
        memmove((void *)&fat_ep->DIR_WrtTime, (void *)&dirent_s_tmp->DIR_WrtTime, sizeof(dirent_s_tmp->DIR_WrtTime));
        memmove((void *)&fat_ep->DIR_WrtDate, (void *)&dirent_s_tmp->DIR_WrtDate, sizeof(dirent_s_tmp->DIR_WrtDate));

        fat_ep->DIR_FileSize = dirent_s_tmp->DIR_FileSize;
        brelse(bp);
        fat_ep->valid = 1;
        if (fat_ep->Attr == 0)
            panic("fat32_fat_entry_lock: no Attr");
    }
}

// 释放fat_entry的锁
void fat32_fat_entry_unlock(fat_entry_t *fat_ep) {
    if (fat_ep == 0 || !holdingsleep(&fat_ep->lock) || fat_ep->ref < 1)
        panic("iunlock");
    releasesleep(&fat_ep->lock);
}

// fat_entry put : trunc and update
void fat32_fat_entry_put(fat_entry_t *fat_ep) {
    acquire(&fat_entry_table.lock);
    if (fat_ep->ref == 1 && fat_ep->valid && fat_ep->nlink == 0) {
        acquiresleep(&fat_ep->lock);

        release(&fat_entry_table.lock);
        fat32_fat_entry_trunc(fat_ep);
        fat_ep->Attr = 0;
        fat32_fat_entry_update(fat_ep);
        fat_ep->valid = 0;

        releasesleep(&fat_ep->lock);

        acquire(&fat_entry_table.lock);
    }

    fat_ep->ref--;
    release(&fat_entry_table.lock);
}

// unlock and put
void fat32_fat_entry_unlock_put(fat_entry_t *fat_ep) {
    fat32_fat_entry_unlock(fat_ep);
    fat32_fat_entry_put(fat_ep);
}

#define fat32_namecmp(s, t) (strncmp(s, t, PATH_LONG_MAX))

int fat32_filter_longname(dirent_l_t *dirent_l_tmp, char *ret_name) {
    int idx = 0;
    for (int i = 0; i < 5; i++) {
        ret_name[idx++] = LONG_NAME_CHAR_MASK(dirent_l_tmp->LDIR_Name1[i]);
        if (!LONG_NAME_CHAR_VALID(dirent_l_tmp->LDIR_Name1[i]))
            return idx;
    }
    for (int i = 0; i < 6; i++) {
        ret_name[idx++] = LONG_NAME_CHAR_MASK(dirent_l_tmp->LDIR_Name2[i]);
        if (!LONG_NAME_CHAR_VALID(dirent_l_tmp->LDIR_Name2[i]))
            return idx;
    }
    for (int i = 0; i < 2; i++) {
        ret_name[idx++] = LONG_NAME_CHAR_MASK(dirent_l_tmp->LDIR_Name3[i]);
        if (!LONG_NAME_CHAR_VALID(dirent_l_tmp->LDIR_Name3[i]))
            return idx;
    }
    return idx;
}

fat_entry_t *fat32_fat_entry_dirlookup(fat_entry_t *fat_ep, char *name, uint *poff) {
    if (!DIR_BOOL((fat_ep->Attr)))
        panic("dirlookup not DIR");
    struct buffer_head *bp;
    FAT_term_t iter_n = fat_ep->cluster_start;
    uint32 FAT_s_n;
    uint32 FAT_s_offset;

    char name_buf[NAME_LONG_MAX];
    memset(name_buf, 0, sizeof(name_buf));
    int name_idx = 0;

    Stack_t fcb_stack;
    stack_init(&fcb_stack);

    int first_sector;
    int off = 0;
    // FAT seek cluster chains
    while (!ISEOF(iter_n)) {
        first_sector = FirstSectorofCluster(iter_n);
        // sectors in a cluster
        for (int s = 0; s < (global_fatfs.sector_per_cluster); s++) {
            bp = bread(global_fatfs.dev, first_sector + s);

            dirent_s_t *fcb_s = (dirent_s_t *)(bp->data);
            dirent_l_t *fcb_l = (dirent_l_t *)(bp->data);
            int idx = 0;
            // FCB in a sector
            while (idx < FCB_PER_BLOCK) {
                // long dirctory item push into the stack
                while (LONG_NAME_BOOL(fcb_l[idx].LDIR_Attr)) {
                    stack_push(&fcb_stack, fcb_l[idx++]);
                    off++;
                }

                // name_idx = 0;
                // reverse the stack to check every long directory entry
                while (!stack_is_empty(&fcb_stack)) {
                    dirent_l_t fcb_l_tmp = stack_pop(&fcb_stack);
                    // check sum
                    if (fcb_l_tmp.LDIR_Chksum != ChkSum(fcb_s[idx].DIR_Name)) {
                        panic("check sum error");
                    }
                    char l_tmp[14];
                    memset(l_tmp,0,sizeof(l_tmp));
                    int l_tmp_len = fat32_filter_longname(&fcb_l_tmp, l_tmp);
                    for (int i = 0; i < l_tmp_len; i++) {
                        name_buf[name_idx++] = l_tmp[i];
                    }
                }
                name_buf[name_idx] = '\0';

                // search for?
                if (namecmp(name, name_buf) == 0) {
                    // entry matches path element
                    if (poff)
                        *poff = off;

                    return fat32_fat_entry_get(global_fatfs.dev, SECTOR_TO_FATNUM(first_sector + s, idx), name, off);
                } else {
                    memset(name_buf, 0, sizeof(name_buf));
                    name_idx = 0;
                }
                idx++;
                off++;
            }
            brelse(bp);
        }
        iter_n = fat32_fat_next_cluster(iter_n);
    }
    return 0;
}

// get a fat_entry , move it from disk to memory
fat_entry_t *fat32_fat_entry_get(uint dev, uint fat_num, char *name, uint parentoff) {
    fat_entry_t *fat_ep, *empty;
    acquire(&fat_entry_table.lock);

    // Is the fat_entry already in the table?
    empty = 0;
    for (fat_ep = fat_entry_table.fat_table; fat_ep < &fat_entry_table.fat_table[NENTRY]; fat_ep++) {
        if (fat_ep->ref > 0 && fat_ep->fatfs_obj->dev == dev && fat_ep->fat_num == fat_num) {
            fat_ep->ref++;
            release(&fat_entry_table.lock);
            return fat_ep;
        }
        if (empty == 0 && fat_ep->ref == 0) // Remember empty slot.
            empty = fat_ep;
    }

    // Recycle an fat32 entry.
    if (empty == 0)
        panic("fat32_fat_entry_get: no space");

    fat_ep = empty;
    fat_ep->fatfs_obj = &global_fatfs;
    fat_ep->fatfs_obj->dev = dev;
    fat_ep->fat_num = fat_num;
    fat_ep->ref = 1;
    fat_ep->valid = 0;
    fat_ep->parent_off = parentoff;
    strncpy(fat_ep->fname, name, strlen(fat_ep));

    release(&fat_entry_table.lock);
    return fat_ep;
}

// find the next cluster of current cluster
uint fat32_fat_next_cluster(uint cluster_curr) {
    struct buffer_head *bp;
    uint FAT_s_n = ThisFATSecNum(cluster_curr);
    uint FAT_s_offset = ThisFATEntOffset(cluster_curr);
    bp = bread(ROOTDEV, FAT_s_n);
    FAT_term_t fat_next = FAT32ClusEntryVal(bp->data, cluster_curr);
    brelse(bp);
    return fat_next;
}

// Read data from fat_entry.
uint fat32_fat_entry_read(fat_entry_t *fat_ep, int user_dst, uint64 dst, uint off, uint n) {
    uint tot = 0, m;
    struct buffer_head *bp;

    // 是一个目录
    int fileSize = 0;
    if (DIR_BOOL(fat_ep->Attr)) {
        // TODO: 把目录的大小考虑进去
        fileSize = DIRLENGTH(fat_ep);
    } else {
        fileSize = fat_ep->DIR_FileSize;
    }

    // 特判合法
    if (off > fileSize || off + n < off)
        return 0;
    if (off + n > fileSize)
        n = fileSize - off;

    FAT_term_t iter_n = fat_ep->cluster_start;

    // find the target cluster of off
    while (!ISEOF(iter_n)) {
        if (LOGISTIC_C_NUM(off) == iter_n)
            break;
        iter_n = fat32_fat_next_cluster(iter_n);
    }
    int init_s_n = LOGISTIC_S_NUM(off);
    int init_s_offset = LOGISTIC_S_OFFSET(off);

    // read the target sector
    while (!ISEOF(iter_n) && tot < n) {
        int first_sector = FirstSectorofCluster(iter_n);
        for (int s = init_s_n; s < fat_ep->fatfs_obj->sector_per_cluster; s++) {
            bp = bread(fat_ep->fatfs_obj->dev, first_sector + s);
            m = MIN(BSIZE - init_s_offset, n - tot);

            if (either_copyout(user_dst, dst, bp->data + init_s_offset, m) == -1) {
                brelse(bp);
                tot = -1;
                break;
            }
            brelse(bp);

            tot += m;
            dst += m;
            init_s_offset = 0;
        }
        init_s_n = 0;

        iter_n = fat32_fat_next_cluster(iter_n);
    }
    return tot;
}

// Write data to fat_entry
uint fat32_fat_entry_write(fat_entry_t *fat_ep, int user_src, uint64 src, uint off, uint n) {
    uint tot = 0, m;
    struct buffer_head *bp;

    // 是一个目录
    int fileSize = 0;
    if (DIR_BOOL(fat_ep->Attr)) {
        // TODO: 把目录的大小考虑进去
        fileSize = DIRLENGTH(fat_ep);
        ;
    } else {
        fileSize = fat_ep->DIR_FileSize;
    }
    if (off > fileSize || off + n < off)
        return -1;
    if (off + n > DataSec * BSIZE)
        return -1;

    FAT_term_t iter_n = fat_ep->cluster_start;

    // find the target cluster of off
    while (!ISEOF(iter_n)) {
        if (LOGISTIC_C_NUM(off) == iter_n)
            break;
        iter_n = fat32_fat_next_cluster(iter_n);
    }

    // TODO: off如果超出文件大小。
    // 这里先假设off不会超出文件大小。

    int init_s_n = LOGISTIC_S_NUM(off);
    int init_s_offset = LOGISTIC_S_OFFSET(off);

    // read the target sector
    while (!ISEOF(iter_n) && tot < n) {
        int first_sector = FirstSectorofCluster(iter_n);
        for (int s = init_s_n; s < fat_ep->fatfs_obj->sector_per_cluster; s++) {
            bp = bread(fat_ep->fatfs_obj->dev, first_sector + s);
            m = MIN(BSIZE - init_s_offset, n - tot);

            if (either_copyin(bp->data + init_s_offset, user_src, src, m) == -1) {
                brelse(bp);
                tot = -1;
                break;
            }
            brelse(bp);
            tot += m;
            src += m;
            init_s_offset = 0;
        }
        init_s_n = 0;
        iter_n = fat32_fat_next_cluster(iter_n);
        if (iter_n == EOC_MASK)
            iter_n = fat32_cluster_alloc(ROOTDEV);
    }
    if (off + n > fat_ep->DIR_FileSize)
        fat_ep->DIR_FileSize = off + n;

    fat32_fat_entry_update(fat_ep);
    return tot;
}

// allocate a fat term
uint fat32_fat_alloc() {
    struct buffer_head *bp;

    int c = 0;
    int sec = FAT_BASE;
    while (c < FAT_CLUSTER_CNT) {
        bp = bread(global_fatfs.dev, sec);
        for (int s = 0; s < FAT_PER_BLOCK; s++) {
            if ((bp->data)[s] == FREE_MASK) {
                brelse(bp);
                return c;
            }
            c++;
        }
        sec++;
        brelse(bp);
    }
    return -1;
}

// allocate a free cluster
uint fat32_cluster_alloc(uint dev) {
    struct buffer_head *bp;
    if (!global_fatfs.free_count) {
        panic("no disk space!!!\n");
    }
    uint free_num = global_fatfs.nxt_free;
    global_fatfs.free_count--;

    // the first sector
    int first_sector = FirstSectorofCluster(global_fatfs.nxt_free);
    bp = bread(dev, FATNUM_TO_SECTOR(free_num));
    if (NAME0_FREE_ALL((bp->data)[0]) && global_fatfs.nxt_free < FAT_CLUSTER_CNT - 1)
        global_fatfs.nxt_free++;
    else {
        uint fat_next = fat32_fat_alloc();
        if (fat_next == -1)
            panic("no more space");
        global_fatfs.nxt_free = fat_next;
    }
    brelse(bp);

    // update fsinfo
    bp = bread(dev, 1);
    fsinfo_t *fsinfo_tmp = (fsinfo_t *)(bp->data);
    fsinfo_tmp->Free_Count = global_fatfs.free_count;
    fsinfo_tmp->Nxt_Free = global_fatfs.nxt_free;
    bwrite(bp);
    brelse(bp);

    return free_num;
}

uint8 fat32_find_same_name_cnt(fat_entry_t *fat_ep, char *name) {
    if (DIR_BOOL((fat_ep->Attr)))
        panic("dirlookup not DIR");

    struct buffer_head *bp;
    FAT_term_t iter_n = fat_ep->cluster_start;
    uint32 FAT_s_n;
    uint32 FAT_s_offset;

    int first_sector;
    int ret = 0;

    to_upper(name);
    // FAT seek cluster chains
    while (!ISEOF(iter_n)) {
        first_sector = FirstSectorofCluster(iter_n);
        // sectors in a cluster
        for (int s = 0; s < (fat_ep->fatfs_obj->sector_per_cluster); s++) {
            bp = bread(fat_ep->fatfs_obj->dev, first_sector + s);
            dirent_s_t *fcb_s = (dirent_s_t *)(bp->data);
            int idx = 0;
            // FCB in a sector
            while (idx < FCB_PER_BLOCK) {
                if (!LONG_NAME_BOOL(fcb_s[idx].DIR_Attr)) {
                    // is our search for?
                    if (fcb_s[idx].DIR_Name[6] != '~' && !strncmp(fcb_s[idx].DIR_Name, name, 6)) {
                        ret++;
                    }
                }
                idx++;
            }
            brelse(bp);
        }
        iter_n = fat32_fat_next_cluster(iter_n);
    }
    return ret;
}

// void fat32_fat_entry_stat(fat_entry_t* fat_ep, struct stat *st) {
//     st->dev = fat_ep->fatfs_obj->dev;
//     // st->ino = fat_ep->inum;
//     st->type = fat_ep->Attr;
//     st->nlink = fat_ep->nlink;
//     st->size = fat_ep->DIR_FileSize;
// }

// int fat32_fat_dirlink(fat_entry_t *fat_ep, char *name, uint inum) {
// int off;
// struct dirent de;
// struct inode *fat_ep;

// // Check that name is not present.
// if ((fat_ep = dirlookup(dp, name, 0)) != 0) {
//     fat_eput(fat_ep);
//     return -1;
// }

// // Look for an empty dirent.
// for (off = 0; off < dp->size; off += sizeof(de)) {
//     if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
//         panic("dirlink read");
//     if (de.inum == 0)
//         break;
// }

// strncpy(de.name, name, DIRSIZ);
// de.inum = inum;
// if (writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
//     return -1;

// return 0;
// }

// uint fat32_fat_entry_dirlength(fat_entry_t* fat_ep)
// {
// struct buffer_head *bp;
// int ret=(fat_ep->cluster_cnt-1)*(fat_ep->fatfs_obj->cluster_size);
// int last_c_first_s =  FirstSectorofCluster(fat_ep->cluster_end);
// for(int s = last_c_first_s;s<fat_ep->fatfs_obj->sector_per_cluster;s++)
// {
//     bp = bread(fat_ep->fatfs_obj->dev,last_c_first_s+s);
//     dirent_s_t* diernt_s_tmp = (dirent_s_t*)(bp->data);
//     for(int i=0;i<FCB_PER_BLOCK;i++)
//     {
//         if(NAME0_FREE_ONLY(diernt_s_tmp[i].DIR_Name[0])||NAME0_FREE_ALL(diernt_s_tmp[i].DIR_Name[0]))
//         {
//             return ret;
//         }else{
//             ret+=sizeof(dirent_s_t);
//         }
//     }
//     brelse(bp);
// }
// return ret;
//     return (fat_ep->cluster_cnt)*global_fatfs.cluster_size;
// }
