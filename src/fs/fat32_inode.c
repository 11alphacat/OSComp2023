#include "common.h"
#include "memory/list_alloc.h"
#include "fs/bio.h"
#include "atomic/spinlock.h"
#include "kernel/trap.h"
#include "fs/fat/fat32_disk.h"
#include "debug.h"
#include "param.h"
#include "fs/fat/fat32_mem.h"
#include "kernel/proc.h"
#include "fs/stat.h"
#include "fs/vfs/fs.h"
#include "fs/fat/fat32_stack.h"

extern struct file_operations fat32_fop;
extern struct inode_operations fat32_iop;

struct _superblock fat32_sb;
// maximum file inode in memory
#define NENTRY 10
struct inode_table_t {
    spinlock_t *lock;
    struct _inode inode_entry[NENTRY];
} inode_table;

void inode_table_init() {
    struct _inode *entry;
    for (entry = inode_table.inode_entry; entry < &inode_table.inode_entry[NENTRY]; entry++) {
        memset(entry, 0, sizeof(struct _inode));
        initsleeplock(&entry->i_sem, "inode_entry");
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
        FAT_entry_t fat_next = FAT32ClusEntryVal(bp->data, iter_n);
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

struct _inode *fat32_root_inode_init(struct _superblock *sb) {
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
    root_ip->i_size = (root_ip->fat32_i.cluster_cnt) * (root_ip->i_sb->s_blocksize);
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

static struct _inode *fat32_inode_namex(char *path, int nameeparent, char *name) {
    struct _inode *ip = NULL, *next = NULL;

    if (*path == '/')
        ip = fat32_inode_dup(&fat32_sb.fat32_sb_info.root_entry);
    else
        ip = fat32_inode_dup(myproc()->cwd);

    while ((path = skepelem(path, name)) != 0) {
        fat32_inode_lock(ip);
        if (!DIR_BOOL(ip->fat32_i.Attr)) {
            fat32_inode_unlock_put(ip);
            return 0;
        }
        if (nameeparent && *path == '\0') {
            // Stop one level early.
            fat32_inode_lock(ip);
            return ip;
        }
        if ((next = fat32_inode_dirlookup(ip, name, 0)) == 0) {
            fat32_inode_put(ip);
            return 0;
        }
        fat32_inode_put(ip);
        ip = next;
    }
    if (nameeparent) {
        fat32_inode_put(ip);
        return 0;
    }
    return ip;
}

struct _inode *fat32_name_inode(char *path) {
    char name[PATH_LONG_MAX]; // 260
    return fat32_inode_namex(path, 0, name);
}

struct _inode *fat32_name_inode_parent(char *path, char *name) {
    return fat32_inode_namex(path, 1, name);
}

struct _inode *fat32_inode_dup(struct _inode *ip) {
    acquire(&inode_table.lock);
    ip->ref++;
    release(&inode_table.lock);
    return ip;
}

void fat32_inode_update(struct _inode *ip) {
    struct buffer_head *bp;
    bp = bread(ip->i_dev, FATINUM_TO_SECTOR(ip->i_ino));
    dirent_s_t *dirent_s_tmp = (dirent_s_t *)bp->data + FATINUM_TO_OFFSET(ip->i_ino);

    dirent_s_tmp->DIR_Attr = ip->fat32_i.Attr;
    dirent_s_tmp->DIR_LstAccDate = ip->fat32_i.DIR_LstAccDate;
    dirent_s_tmp->DIR_WrtDate = ip->fat32_i.DIR_WrtDate;
    dirent_s_tmp->DIR_WrtTime = ip->fat32_i.DIR_WrtTime;
    // memmove((void *)&dirent_s_tmp->DIR_LstAccDate, (void *)&ip->fat32_i.DIR_LstAccDate, sizeof(ip->fat32_i.DIR_LstAccDate));
    // memmove((void *)&dirent_s_tmp->DIR_WrtDate, (void *)&ip->fat32_i.DIR_WrtDate, sizeof(ip->fat32_i.DIR_WrtDate));
    // memmove((void *)&dirent_s_tmp->DIR_WrtTime, (void *)&ip->fat32_i.DIR_WrtTime, sizeof(ip->fat32_i.DIR_WrtTime));
    dirent_s_tmp->DIR_FstClusHI = DIR_FIRST_HIGH(ip->fat32_i.cluster_start);
    dirent_s_tmp->DIR_FstClusLO = DIR_FIRST_LOW(ip->fat32_i.cluster_start);
    dirent_s_tmp->DIR_FileSize = ip->fat32_i.DIR_FileSize;

    bwrite(bp);
    brelse(bp);
}

void fat32_inode_trunc(struct _inode *ip) {
    struct buffer_head *bp;

    FAT_entry_t iter_n = ip->fat32_i.cluster_start;
    FAT_entry_t end = 0;
    uint32 FAT_sector_num;
    uint32 FAT_sector_offset;

    // truncate the data
    while (!ISEOF(iter_n)) {
        FAT_entry_t fat_next = fat32_next_cluster(iter_n);
        bp = bread(ip->i_dev, iter_n);
        SetFAT32ClusEntryVal(bp->data, iter_n, FREE_MASK);
        iter_n = fat_next;
        bwrite(bp);
        brelse(bp);
    }
    ip->fat32_i.cluster_start = 0;
    ip->fat32_i.cluster_end = 0;
    ip->fat32_i.parent_off = 0;
    ip->fat32_i.cluster_cnt = 0;
    ip->fat32_i.DIR_FileSize = 0;
    fat32_inode_update(ip);
}

// find the next cluster of current cluster
uint fat32_next_cluster(uint cluster_cur) {
    ASSERT(cluster_cur >= 2 && cluster_cur < FAT_CLUSTER_MAX);
    struct buffer_head *bp;
    uint sector_num = ThisFATEntSecNum(cluster_cur);
    uint sector_offset = ThisFATEntOffset(cluster_cur);
    bp = bread(fat32_sb.s_dev, sector_num);
    FAT_entry_t fat_next = FAT32ClusEntryVal(bp->data, cluster_cur);
    brelse(bp);
    return fat_next;
}

// 获取fat32 inode的锁
void fat32_inode_lock(struct _inode *ip) {
    struct buffer_head *bp;
    if (ip == 0 || ip->ref < 1)
        panic("ilock");
    acquiresleep(&ip->i_sem);
    if (ip->valid == 0) {
        bp = bread(ip->i_dev, FATINUM_TO_SECTOR(ip->i_ino));
        dirent_s_t *dirent_s_tmp = (dirent_s_t *)bp->data + FATINUM_TO_OFFSET(ip->i_ino);

        ip->fat32_i.Attr = dirent_s_tmp->DIR_Attr;
        ip->fat32_i.cluster_start = DIR_FIRST_CLUS(dirent_s_tmp->DIR_FstClusHI, dirent_s_tmp->DIR_FstClusLO);
        ip->fat32_i.cluster_cnt = fat32_fat_travel(ip->fat32_i.cluster_start, &(ip->fat32_i.cluster_end));
        ip->fat32_i.DIR_CrtTimeTenth = dirent_s_tmp->DIR_CrtTimeTenth;

        memmove((void *)&ip->fat32_i.DIR_CrtTime, (void *)&dirent_s_tmp->DIR_CrtTime, sizeof(dirent_s_tmp->DIR_CrtTime));
        memmove((void *)&ip->fat32_i.DIR_CrtDate, (void *)&dirent_s_tmp->DIR_CrtDate, sizeof(dirent_s_tmp->DIR_CrtDate));
        memmove((void *)&ip->fat32_i.DIR_LstAccDate, (void *)&dirent_s_tmp->DIR_LstAccDate, sizeof(dirent_s_tmp->DIR_LstAccDate));
        memmove((void *)&ip->fat32_i.DIR_WrtTime, (void *)&dirent_s_tmp->DIR_WrtTime, sizeof(dirent_s_tmp->DIR_WrtTime));
        memmove((void *)&ip->fat32_i.DIR_WrtDate, (void *)&dirent_s_tmp->DIR_WrtDate, sizeof(dirent_s_tmp->DIR_WrtDate));

        ip->fat32_i.DIR_FileSize = dirent_s_tmp->DIR_FileSize;
        brelse(bp);
        ip->valid = 1;
        if (ip->fat32_i.Attr == 0)
            panic("fat32_inode_lock: no Attr");
    }
}

// 释放fat32 inode的锁
void fat32_inode_unlock(struct _inode *ip) {
    if (ip == 0 || !holdingsleep(&ip->i_sem) || ip->ref < 1)
        panic("iunlock");
    releasesleep(&ip->i_sem);
}

// fat32 inode put : trunc and update
void fat32_inode_put(struct _inode *ip) {
    acquire(&ip->i_sem);
    if (ip->ref == 1 && ip->valid && ip->i_nlink == 0) {
        acquiresleep(&ip->i_sem);

        release(&inode_table.lock);
        fat32_inode_trunc(ip);
        ip->fat32_i.Attr = 0;
        fat32_inode_update(ip);
        ip->valid = 0;

        releasesleep(&ip->i_sem);

        acquire(&inode_table.lock);
    }

    ip->ref--;
    release(&inode_table.lock);
}

// unlock and put
void fat32_inode_unlock_put(struct _inode *ip) {
    fat32_inode_unlock(ip);
    fat32_inode_put(ip);
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

struct _inode *fat32_inode_dirlookup(struct _inode *ip, char *name, uint *poff) {
    if (!DIR_BOOL((ip->fat32_i.Attr)))
        panic("dirlookup not DIR");
    struct buffer_head *bp;
    FAT_entry_t iter_n = ip->fat32_i.cluster_start;
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
        for (int s = 0; s < (ip->i_sb->sectors_per_block); s++) {
            bp = bread(ip->i_dev, first_sector + s);

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

                // reverse the stack to check every long directory entry
                while (!stack_is_empty(&fcb_stack)) {
                    dirent_l_t fcb_l_tmp = stack_pop(&fcb_stack);
                    // check sum
                    if (fcb_l_tmp.LDIR_Chksum != ChkSum(fcb_s[idx].DIR_Name)) {
                        panic("check sum error");
                    }
                    char l_tmp[14];
                    memset(l_tmp, 0, sizeof(l_tmp));
                    int l_tmp_len = fat32_filter_longname(&fcb_l_tmp, l_tmp);
                    for (int i = 0; i < l_tmp_len; i++) {
                        name_buf[name_idx++] = l_tmp[i];
                    }
                }
                name_buf[name_idx] = '\0';

                // search for?
                if (namecmp(name, name_buf) == 0) {
                    // inode matches path element
                    if (poff)
                        *poff = off;

                    return fat32_inode_get(ip->i_dev, SECTOR_TO_FATINUM(first_sector + s, idx), name, off);
                } else {
                    memset(name_buf, 0, sizeof(name_buf));
                    name_idx = 0;
                }
                idx++;
                off++;
            }
            brelse(bp);
        }
        iter_n = fat32_next_cluster(iter_n);
    }
    return 0;
}

// get a inode , move it from disk to memory
struct _inode *fat32_inode_get(uint dev, uint inum, char *name, uint parentoff) {
    struct _inode *ip, *empty;
    acquire(&inode_table.lock);

    // Is the fat32 inode already in the table?
    empty = 0;
    for (ip = inode_table.inode_entry; ip < &inode_table.inode_entry[NENTRY]; ip++) {
        if (ip->ref > 0 && ip->i_dev == dev && ip->i_ino == inum) {
            ip->ref++;
            release(&inode_table.lock);
            return ip;
        }
        if (empty == 0 && ip->ref == 0) // Remember empty slot.
            empty = ip;
    }

    // Recycle an fat32 entry.
    if (empty == 0)
        panic("fat32_inode_get: no space");

    ip = empty;
    ip->i_sb = &fat32_sb;
    ip->i_dev = dev;
    ip->i_ino = inum;
    ip->ref = 1;
    ip->valid = 0;
    ip->fat32_i.parent_off = parentoff;
    strncpy(ip->fat32_i.fname, name, strlen(ip));

    release(&inode_table.lock);
    return ip;
}

// Read data from inode.
uint fat32_inode_read(struct _inode *ip, int user_dst, uint64 dst, uint off, uint n) {
    uint tot = 0, m;
    struct buffer_head *bp;

    // 是一个目录
    int fileSize = 0;
    if (DIR_BOOL(ip->fat32_i.Attr)) {
        // TODO: 把目录的大小考虑进去
        fileSize = DIRLENGTH(ip);
    } else {
        fileSize = ip->fat32_i.DIR_FileSize;
    }

    // 特判合法
    if (off > fileSize || off + n < off)
        return 0;
    if (off + n > fileSize)
        n = fileSize - off;

    FAT_entry_t iter_n = ip->fat32_i.cluster_start;

    // find the target cluster of off
    while (!ISEOF(iter_n)) {
        if (LOGISTIC_C_NUM(off) == iter_n)
            break;
        iter_n = fat32_next_cluster(iter_n);
    }
    int init_s_n = LOGISTIC_S_NUM(off);
    int init_s_offset = LOGISTIC_S_OFFSET(off);

    // read the target sector
    while (!ISEOF(iter_n) && tot < n) {
        int first_sector = FirstSectorofCluster(iter_n);
        for (int s = init_s_n; s < (ip->i_sb->sectors_per_block); s++) {
            bp = bread(ip->i_sb, first_sector + s);
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

        iter_n = fat32_next_cluster(iter_n);
    }
    return tot;
}

// Write data to fat32 inode
uint fat32_inode_write(struct _inode *ip, int user_src, uint64 src, uint off, uint n) {
    uint tot = 0, m;
    struct buffer_head *bp;

    // 是一个目录
    int fileSize = 0;
    if (DIR_BOOL(ip->fat32_i.Attr)) {
        // TODO: 把目录的大小考虑进去
        fileSize = DIRLENGTH(ip);
        ;
    } else {
        fileSize = ip->fat32_i.DIR_FileSize;
    }
    if (off > fileSize || off + n < off)
        return -1;
    if (off + n > DataSec * BSIZE)
        return -1;

    FAT_entry_t iter_n = ip->fat32_i.cluster_start;

    // find the target cluster of off
    while (!ISEOF(iter_n)) {
        if (LOGISTIC_C_NUM(off) == iter_n)
            break;
        iter_n = fat32_next_cluster(iter_n);
    }

    // TODO: off如果超出文件大小。
    // 这里先假设off不会超出文件大小。

    int init_s_n = LOGISTIC_S_NUM(off);
    int init_s_offset = LOGISTIC_S_OFFSET(off);

    // read the target sector
    while (!ISEOF(iter_n) && tot < n) {
        int first_sector = FirstSectorofCluster(iter_n);
        for (int s = init_s_n; s < ip->i_sb->sectors_per_block; s++) {
            bp = bread(ip->i_dev, first_sector + s);
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
        iter_n = fat32_next_cluster(iter_n);
        if (iter_n == EOC)
            iter_n = fat32_cluster_alloc(ROOTDEV);
    }
    if (off + n > ip->fat32_i.DIR_FileSize)
        ip->fat32_i.DIR_FileSize = off + n;

    fat32_inode_update(ip);
    return tot;
}

// allocate a fat term
uint fat32_fat_alloc() {
    struct buffer_head *bp;

    int c = 0;
    int sec = FAT_BASE;
    while (c < FAT_CLUSTER_MAX) {
        bp = bread(fat32_sb.s_dev, sec);
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
    if (!fat32_sb.fat32_sb_info.free_count) {
        panic("no disk space!!!\n");
    }
    uint free_num = fat32_sb.fat32_sb_info.nxt_free;
    fat32_sb.fat32_sb_info.free_count--;

    // the first sector
    int first_sector = FirstSectorofCluster(fat32_sb.fat32_sb_info.nxt_free);
    bp = bread(dev, FATINUM_TO_SECTOR(free_num));
    if (NAME0_FREE_ALL((bp->data)[0]) && fat32_sb.fat32_sb_info.nxt_free < FAT_CLUSTER_MAX - 1)
        fat32_sb.fat32_sb_info.nxt_free++;
    else {
        uint fat_next = fat32_fat_alloc();
        if (fat_next == -1)
            panic("no more space");
        fat32_sb.fat32_sb_info.nxt_free = fat_next;
    }
    brelse(bp);

    // update fsinfo
    bp = bread(dev, 1);
    fsinfo_t *fsinfo_tmp = (fsinfo_t *)(bp->data);
    fsinfo_tmp->Free_Count = fat32_sb.fat32_sb_info.free_count;
    fsinfo_tmp->Nxt_Free = fat32_sb.fat32_sb_info.nxt_free;
    bwrite(bp);
    brelse(bp);

    return free_num;
}

uint fat32_find_same_name_cnt(struct _inode *ip, char *name) {
    if (!DIR_BOOL((ip->fat32_i.Attr)))
        panic("dirlookup not DIR");

    struct buffer_head *bp;
    FAT_entry_t iter_n = ip->fat32_i.cluster_start;
    uint32 FAT_s_n;
    uint32 FAT_s_offset;

    int first_sector;
    int ret = 0;

    str_toupper(name);
    // FAT seek cluster chains
    while (!ISEOF(iter_n)) {
        first_sector = FirstSectorofCluster(iter_n);
        // sectors in a cluster
        for (int s = 0; s < (ip->i_sb->sectors_per_block); s++) {
            bp = bread(ip->i_dev, first_sector + s);
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
        iter_n = fat32_next_cluster(iter_n);
    }
    return ret;
}

int fat32_fcb_init(struct _inode *ip_parent, uchar *long_name, uchar attr, char *fcb_char) {
    dirent_s_t dirent_s_cur;
    memset((void *)&dirent_s_cur, 0, sizeof(dirent_s_cur));
    // dirent_l_t dirent_l_cur;
    dirent_s_cur.DIR_Attr = attr;

    uint long_idx = -1;

    char file_name[NAME_LONG_MAX];
    char file_ext[4];

    /*short dirent*/
    int name_len = strlen(long_name);
    // 数据文件
    if (!DIR_BOOL(attr)) {
        if (str_split(long_name, '.', file_name, file_ext) == -1) {
            panic("fcb init : str split");
        }
        str_toupper(file_ext);
        strncpy(dirent_s_cur.DIR_Name + 8, file_ext, 3); // extend name

        str_toupper(file_name);
        strncpy(dirent_s_cur.DIR_Name, file_name, 8);
        if (strlen(long_name) > 8) {
            long_idx = fat32_find_same_name_cnt(ip_parent, long_name);
            dirent_s_cur.DIR_Name[6] = '~';
            dirent_s_cur.DIR_Name[7] = long_idx;
        }
    } else {
        // 目录文件
        strncpy(file_name, long_name, 11);

        str_toupper(file_name);
        strncpy(dirent_s_cur.DIR_Name, file_name, 11);
        if (strlen(long_name) > 8) {
            long_idx = fat32_find_same_name_cnt(ip_parent, long_name);

            strncpy(dirent_s_cur.DIR_Name + 8, file_name + (name_len - 3), 3); // last three char
            dirent_s_cur.DIR_Name[6] = '~';
            dirent_s_cur.DIR_Name[7] = long_idx;
        }
    }
    dirent_s_cur.DIR_FileSize = 0;

    uint first_c = fat32_cluster_alloc(ip_parent->i_dev);
    dirent_s_cur.DIR_FstClusHI = DIR_FIRST_HIGH(first_c);
    dirent_s_cur.DIR_FstClusLO = DIR_FIRST_LOW(first_c);

    /*push long dirent into stack*/
    Stack_t fcb_stack;
    stack_init(&fcb_stack);
    int iter_order = 1;
    uchar checksum = ChkSum(dirent_s_cur.DIR_Name);
    int char_idx = 0;
    // every long name entry
    for (int i = 1; i <= name_len / FAT_LFN_LENGTH + 1; i++) {
        dirent_l_t dirent_l_cur;
        memset((void *)&(dirent_l_cur), 0xFF, sizeof(dirent_l_cur));
        if (char_idx == name_len)
            break;

        // order
        if (i == name_len / FAT_LFN_LENGTH + 1)
            dirent_l_cur.LDIR_Ord = LAST_LONG_ENTRY_SET(i);
        else
            dirent_l_cur.LDIR_Ord = i;

        // Name
        int end_flag = 0;
        for (int i = 0; i < 5 && !end_flag; i++) {
            dirent_l_cur.LDIR_Name1[i] = LONG_NAME_CHAR_SET(long_name[char_idx]);
            if (LONG_NAME_CHAR_VALID(long_name[char_idx++]))
                end_flag = 1;
        }
        for (int i = 0; i < 6 && !end_flag; i++) {
            dirent_l_cur.LDIR_Name2[i] = LONG_NAME_CHAR_SET(long_name[char_idx]);
            if (LONG_NAME_CHAR_VALID(long_name[char_idx++]))
                end_flag = 1;
        }
        for (int i = 0; i < 2 && !end_flag; i++) {
            dirent_l_cur.LDIR_Name3[i] = LONG_NAME_CHAR_SET(long_name[char_idx]);
            if (LONG_NAME_CHAR_VALID(long_name[char_idx++]))
                end_flag = 1;
        }

        // Attr  and  Type
        dirent_l_cur.LDIR_Attr = ATTR_LONG_NAME_MASK;
        dirent_l_cur.LDIR_Type = 0;

        // check sum
        dirent_l_cur.LDIR_Chksum = checksum;
        stack_push(&fcb_stack, dirent_l_cur);

        dirent_l_cur.LDIR_FstClusLO = 0;
    }

    while (!stack_is_empty(&fcb_stack)) {
        dirent_l_t fcb_l_tmp = stack_pop(&fcb_stack);
        memmove(fcb_char, &fcb_l_tmp, sizeof(fcb_l_tmp));
        fcb_char = fcb_char + sizeof(fcb_l_tmp);
    }
    memmove(fcb_char, &dirent_s_cur, sizeof(dirent_s_cur));
    return 1;
    // TODO: 获取当前时间和日期，还有TimeTenth
}

uchar ChkSum(uchar *pFcbName) {
    uchar Sum = 0;
    for (short FcbNameLen = 11; FcbNameLen != 0; FcbNameLen--) {
        Sum = ((Sum & 1) ? 0x80 : 0) + (Sum >> 1) + *pFcbName++;
    }
    return Sum;
}

struct _inode *fat32_inode_create(char *path, uchar attr) {
    struct _inode *ip, *dp;
    char name[DIRSIZ];

    if ((dp = nameiparent(path, name)) == 0)
        return 0;

    //     ilock(dp);

    //     if ((ip = dirlookup(dp, name, 0)) != 0) {
    //         iunlockput(dp);
    //         ilock(ip);
    //         if (type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE))
    //             return ip;
    //         iunlockput(ip);
    //         return 0;
    //     }

    //     if ((ip = ialloc(dp->dev, type)) == 0) {
    //         iunlockput(dp);
    //         return 0;
    //     }

    //     ilock(ip);
    //     ip->major = major;
    //     ip->minor = minor;
    //     ip->nlink = 1;
    //     iupdate(ip);

    //     if (type == T_DIR) { // Create . and .. entries.
    //         // No ip->nlink++ for ".": avoid cyclic ref count.
    //         if (dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
    //             goto fail;
    //     }

    //     if (dirlink(dp, name, ip->inum) < 0)
    //         goto fail;

    //     if (type == T_DIR) {
    //         // now that success is guaranteed:
    //         dp->nlink++; // for ".."
    //         iupdate(dp);
    //     }

    //     iunlockput(dp);

    //     return ip;

    // fail:
    //     // something went wrong. de-allocate ip.
    //     ip->nlink = 0;
    //     iupdate(ip);
    //     iunlockput(ip);
    //     iunlockput(dp);
    //     return 0;
}

// int fat32_fat_dirlink(fat_entry_t *ip, char *name, uint inum) {
// int off;
// struct dirent de;
// struct inode *ip;

// // Check that name is not present.
// if ((ip = dirlookup(dp, name, 0)) != 0) {
//     iput(ip);
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

// void fat32_fat_entry_stat(fat_entry_t* ip, struct stat *st) {
//     st->dev = ip->fatfs_obj->dev;
//     // st->ino = ip->inum;
//     st->type = ip->Attr;
//     st->nlink = ip->nlink;
//     st->size = ip->DIR_FileSize;
// }

// uint fat32_fat_entry_dirlength(fat_entry_t* ip)
// {
// struct buffer_head *bp;
// int ret=(ip->cluster_cnt-1)*(ip->fatfs_obj->cluster_size);
// int last_c_first_s =  FirstSectorofCluster(ip->cluster_end);
// for(int s = last_c_first_s;s<ip->fatfs_obj->sector_per_cluster;s++)
// {
//     bp = bread(ip->fatfs_obj->dev,last_c_first_s+s);
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
//     return (ip->cluster_cnt)*global_fatfs.cluster_size;
// }
