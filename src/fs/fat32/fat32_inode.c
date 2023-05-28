#include "common.h"
#include "debug.h"
#include "atomic/spinlock.h"
#include "memory/allocator.h"
#include "proc/pcb_life.h"
#include "fs/bio.h"
#include "fs/stat.h"
#include "fs/vfs/fs.h"
#include "fs/vfs/ops.h"
#include "fs/fat/fat32_stack.h"
#include "fs/fat/fat32_mem.h"
#include "fs/fat/fat32_disk.h"
#include "kernel/trap.h"
#include "ctype.h"

extern struct file_operations fat32_fop;
extern struct inode_operations fat32_iop;

struct _superblock fat32_sb;
// maximum file inode in memory
#define NENTRY 50
struct inode_table_t {
    spinlock_t lock;
    struct inode inode_entry[NENTRY];
} inode_table;

/* inefficient, use for debug only! */
static void print_rawstr(const char *c, size_t len) {
    for (int i = 0; i < len; i++) {
        if (*(c + i) == ' ') {
            printf(" ");
        } else if (isalnum(*(c + i)) || *(c + i) == '.' || *(c + i) == '~' || *(c + i) == '_') {
            printf("%c", *(c + i));
        } else {
            printf(" ");
        }
    }
}

static void print_short_dir(const dirent_s_t *buf) {
    printf("(short) ");
    printf("name: ", buf->DIR_Name);
    print_rawstr((char *)buf->DIR_Name, FAT_SFN_LENGTH);
    printf("  ");
    printf("attr: %#x\t", buf->DIR_Attr);
    printf("dev: %d\t", buf->DIR_Dev);
    printf("filesize %d\n", buf->DIR_FileSize);
    // printf("create_time_tenth %d\t", buf->DIR_CrtTimeTenth);
    // printf("create_time %d\n", buf->DIR_CrtTime);
    // printf("crea");
}

static void print_long_dir(const dirent_l_t *buf) {
    printf("(long) ");
    printf("name1: ");
    print_rawstr((char *)buf->LDIR_Name1, 10);
    printf("  ");
    printf("name2: ");
    print_rawstr((char *)buf->LDIR_Name2, 12);
    printf("  ");
    printf("name3: ");
    print_rawstr((char *)buf->LDIR_Name3, 4);
    printf("  ");
    printf("attr %#x\n", buf->LDIR_Attr);
}

void sys_print_rawfile(void) {
    int fd;
    int printdir;
    struct file *f;

    printfGreen("==============\n");
    if (argfd(0, &fd, &f) < 0) {
        printfGreen("file doesn't open!\n");
        return;
    }
    argint(1, &printdir);

    ASSERT(f->f_type == FD_INODE);
    int cnt = 0;
    int pos = 0;
    uint64 iter_n = f->f_tp.f_inode->fat32_i.cluster_start;

    printfGreen("fd is %d\n", fd);
    struct inode *ip = f->f_tp.f_inode;
    if (ip->i_type != T_DIR && printdir == 1) {
        printfGreen("the file is not a directory\n");
        return;
    }
    int off = 0;
    // print logistic clu.no and address(in fat32.img)
    while (!ISEOF(iter_n)) {
        uint64 addr = (iter_n - fat32_sb.fat32_sb_info.root_cluster_s) * __BPB_BytsPerSec * __BPB_SecPerClus + FSIMG_STARTADDR;
        printfGreen("cluster no: %d\t address: %p \toffset %d(%#p)\n", cnt++, addr, pos, pos);
        if (printdir == 1) {
            int first_sector = FirstSectorofCluster(iter_n);
            int init_s_n = LOGISTIC_S_NUM(pos);
            struct buffer_head *bp;
            for (int s = init_s_n; s < __BPB_SecPerClus; s++) {
                bp = bread(ip->i_dev, first_sector + s);
                for (int i = 0; i < 512 && i < ip->i_size; i += 32) {
                    dirent_s_t *tmp = (dirent_s_t *)(bp->data + i);
                    printf("%x ", off++);
                    if (LONG_NAME_BOOL(tmp->DIR_Attr)) {
                        print_long_dir((dirent_l_t *)(bp->data + i));
                    } else {
                        print_short_dir((dirent_s_t *)(bp->data + i));
                    }
                }
                printfRed("===Sector %d end===\n", s);
                brelse(bp);
            }
        }
        pos += __BPB_BytsPerSec * __BPB_SecPerClus;
        iter_n = fat32_next_cluster(iter_n);
    }
    printfGreen("file size is %d(%#p)\n", f->f_tp.f_inode->i_size, f->f_tp.f_inode->i_size);
    printfGreen("==============\n");
    return;
}

// init the global inode table
void inode_table_init() {
    struct inode *entry;
    initlock(&inode_table.lock, "inode_table"); // !!!!
    for (entry = inode_table.inode_entry; entry < &inode_table.inode_entry[NENTRY]; entry++) {
        memset(entry, 0, sizeof(struct inode));
        sema_init(&entry->i_sem, 1, "inode_entry_sem");
        // sema_init(&entry->i_sem, "inode_entry");
    }
}

struct inode *fat32_root_inode_init(struct _superblock *sb) {
    // root inode initialization
    struct inode *root_ip = (struct inode *)kalloc();
    sema_init(&root_ip->i_sem, 1, "fat_root_inode");
    root_ip->i_dev = sb->s_dev;
    root_ip->i_mode = IMODE_NONE;
    // set root inode num to 0
    root_ip->i_ino = 0;
    root_ip->ref = 0;
    root_ip->valid = 1;
    // file size
    root_ip->i_mount = root_ip;
    root_ip->i_sb = sb;
    root_ip->i_nlink = 1;
    root_ip->i_op = get_inodeops[FAT32]();
    root_ip->fs_type = FAT32;

    // the parent of the root is itself
    root_ip->parent = root_ip;
    root_ip->fat32_i.cluster_start = 2;
    root_ip->fat32_i.parent_off = 0;
    root_ip->fat32_i.fname[0] = '/';
    root_ip->fat32_i.fname[1] = '\0';

    // root inode doesn't have these fields, so set these to 0
    root_ip->i_atime = 0;
    root_ip->i_mtime = 0;
    root_ip->i_ctime = 0;

    root_ip->fat32_i.cluster_cnt = fat32_fat_travel(root_ip, 0);
    root_ip->i_size = DIRLENGTH(root_ip);
    root_ip->i_sb = sb;
    root_ip->fat32_i.DIR_FileSize = 0;
    root_ip->i_type = T_DIR;
    DIR_SET(root_ip->fat32_i.Attr);

    ASSERT(root_ip->fat32_i.cluster_start == 2);

    return root_ip;
}

// num is a logical cluster number(within a file)
// start from 1
// num == 0 ~ return count of clusters, set the inode->fat32_i.cluster_end at the same time
// num != 0 ~ return the num'th physical cluster num
uint32 fat32_fat_travel(struct inode *ip, uint num) {
    FAT_entry_t iter_n = ip->fat32_i.cluster_start;
    int cnt = 0;
    int prev = 0;
    while (!ISEOF(iter_n) && (num > ++cnt || num == 0)) {
        prev = iter_n;
        iter_n = fat32_next_cluster(iter_n);
    }

    if (num == 0) {
        ip->fat32_i.cluster_end = prev;
        return cnt;
    } else {
        if (num > cnt)
            return prev;
        return iter_n;
    }
}

// find the next cluster of current cluster
uint fat32_next_cluster(uint cluster_cur) {
    ASSERT(cluster_cur >= 2 && cluster_cur < FAT_CLUSTER_MAX);
    struct buffer_head *bp;
    uint sector_num = ThisFATEntSecNum(cluster_cur);
    // uint sector_offset = ThisFATEntOffset(cluster_cur);
    bp = bread(fat32_sb.s_dev, sector_num);
    FAT_entry_t fat_next = FAT32ClusEntryVal(bp->data, cluster_cur);
    brelse(bp);
    return fat_next;
}

// allocate a free cluster
uint fat32_cluster_alloc(uint dev) {
    struct buffer_head *bp;

    sema_wait(&fat32_sb.sem);
    if (!fat32_sb.fat32_sb_info.free_count) {
        panic("no disk space!!!\n");
    }
    uint free_num = fat32_sb.fat32_sb_info.nxt_free;
    fat32_sb.fat32_sb_info.free_count--;

    // the first sector
    int first_sector = FirstSectorofCluster(fat32_sb.fat32_sb_info.nxt_free);
    bp = bread(dev, first_sector);

    if (NAME0_FREE_ALL((bp->data)[0]) && fat32_sb.fat32_sb_info.nxt_free < FAT_CLUSTER_MAX - 1) {
        // next free hit
        brelse(bp); // !!!!

        fat32_fat_set(fat32_sb.fat32_sb_info.nxt_free, EOC);
        fat32_sb.fat32_sb_info.nxt_free++;
    } else {
        // start from the begin of fat
        brelse(bp); // !!!!

        uint fat_next = fat32_fat_alloc();
        if (fat_next == 0)
            panic("no more space");
        fat32_sb.fat32_sb_info.nxt_free = fat_next + 1;
    }
    sema_signal(&fat32_sb.sem);

    // update fsinfo
    fsinfo_t *fsinfo_tmp;
    bp = bread(dev, SECTOR_FSINFO);
    fsinfo_tmp = (fsinfo_t *)(bp->data);
    fsinfo_tmp->Free_Count = fat32_sb.fat32_sb_info.free_count;
    fsinfo_tmp->Nxt_Free = fat32_sb.fat32_sb_info.nxt_free;
    bwrite(bp);
    brelse(bp);

    // update fsinfo backup
    bp = bread(dev, SECTOR_FSINFO_BACKUP);
    fsinfo_tmp = (fsinfo_t *)(bp->data);
    fsinfo_tmp->Free_Count = fat32_sb.fat32_sb_info.free_count;
    fsinfo_tmp->Nxt_Free = fat32_sb.fat32_sb_info.nxt_free;
    bwrite(bp);
    brelse(bp);

    fat32_zero_cluster(free_num);
    return free_num;
}

// allocate a new fat entry
uint fat32_fat_alloc() {
    struct buffer_head *bp;

    int c = 3;
    int sec = FAT_BASE;
    while (c < FAT_CLUSTER_MAX) {
        bp = bread(fat32_sb.s_dev, sec);
        FAT_entry_t *fats = (FAT_entry_t *)(bp->data);
        for (int s = 0; s < FAT_PER_SECTOR; s++) {
            if (fats[s] == FREE_MASK) {
                brelse(bp); // !!!!
                fat32_fat_set(c, EOC);
                return c;
            }
            c++;
            if (c > FAT_CLUSTER_MAX) {
                brelse(bp);
                return 0;
            }
        }
        sec++;
        brelse(bp);
    }
    return 0;
}

// set fat to value
void fat32_fat_set(uint cluster, uint value) {
    struct buffer_head *bp;
    uint sector_num = ThisFATEntSecNum(cluster);
    bp = bread(fat32_sb.s_dev, sector_num);
    // uint offset = ThisFATEntOffset(cluster);
    SetFAT32ClusEntryVal(bp->data, cluster, value);
    bwrite(bp);
    brelse(bp);
}

// // Examples:
// //   skepelem("a/bb/c", name) = "bb/c", setting name = "a"
// //   skepelem("///a//bb", name) = "bb", setting name = "a"
// //   skepelem("a", name) = "", setting name = "a"
// //   skepelem("", name) = skepelem("////", name) = 0

// //   skepelem("./mnt", name) = "", setting name = "mnt"
// //   skepelem("../mnt", name) = "", setting name = "mnt"
// //   skepelem("..", name) = "", setting name = 0
// static char *skepelem(char *path, char *name) {
//     char *s;
//     int len;

//     while (*path == '/' || *path == '.')
//         path++;
//     if (*path == 0)
//         return 0;
//     s = path;
//     while (*path != '/' && *path != 0)
//         path++;
//     len = path - s;
//     if (len >= PATH_LONG_MAX)
//         memmove(name, s, PATH_LONG_MAX);
//     else {
//         memmove(name, s, len);
//         name[len] = 0;
//     }
//     while (*path == '/')
//         path++;
//     return path;
// }

// struct inode *namei(char *path) {
//     char name[NAME_LONG_MAX];
//     return fat32_inode_namex(path, 0, name);
// }

// struct inode *namei_parent(char *path, char *name) {
//     return fat32_inode_namex(path, 1, name);
// }

// Read data from fa32 inode.
ssize_t fat32_inode_read(struct inode *ip, int user_dst, uint64 dst, uint off, uint n) {
    uint tot = 0, m;
    struct buffer_head *bp;

    // 是一个目录
    int fileSize = ip->i_size;

    // 特判合法
    if (off > fileSize || off + n < off)
        return 0;
    if (off + n > fileSize)
        n = fileSize - off;

    FAT_entry_t iter_n = ip->fat32_i.cluster_start;

    uint C_NUM_off = LOGISTIC_C_NUM(off) + 1;
    // find the target cluster of off
    iter_n = fat32_fat_travel(ip, C_NUM_off);

    int init_s_n = LOGISTIC_S_NUM(off);
    int init_s_offset = LOGISTIC_S_OFFSET(off);

    ssize_t ret = 0;
    // read the target sector
    while (!ISEOF(iter_n) && tot < n) {
        int first_sector = FirstSectorofCluster(iter_n);
        for (int s = init_s_n; s < (ip->i_sb->sectors_per_block) && tot < n; s++) {
            bp = bread(ip->i_dev, (uint)first_sector + s);
            m = MIN(BSIZE - init_s_offset, n - tot);

            if (either_copyout(user_dst, dst, bp->data + init_s_offset, m) == -1) {
                brelse(bp);
                ret = -1;
                break;
            }
            brelse(bp);

            tot += m;
            dst += m;
            init_s_offset = 0;
        }
        if (tot == n || ret == -1)
            break;
        init_s_n = 0;

        iter_n = fat32_next_cluster(iter_n);
    }
    if (ret == 0)
        return tot;
    else
        return ret;
}

// Write data to fat32 inode
// 写 inode 文件，从偏移量 off 起， 写 src 的 n 个字节的内容
ssize_t fat32_inode_write(struct inode *ip, int user_src, uint64 src, uint off, uint n) {
    uint tot = 0, m;
    struct buffer_head *bp;

    // 是一个目录
    int fileSize = ip->i_size;

    if (off > fileSize || off + n < off)
        return -1;
    if (off + n > DataSec * ip->i_sb->sector_size)
        return -1;

    FAT_entry_t iter_n = ip->fat32_i.cluster_start;

    FAT_entry_t next;
    uint C_NUM_off = LOGISTIC_C_NUM(off) + 1;
    // find the target cluster of off
    iter_n = fat32_fat_travel(ip, C_NUM_off);

    if (C_NUM_off > ip->fat32_i.cluster_cnt) {
        FAT_entry_t fat_new = fat32_cluster_alloc(ROOTDEV);
        fat32_fat_set(iter_n, fat_new);

        // uint first_sector = FirstSectorofCluster(fat_new); // debug
        // uint sec_pos = DEBUG_SECTOR(ip, first_sector); // debug
        // printf("%d\n",sec_pos); // debug
        iter_n = fat_new;
        ip->fat32_i.cluster_cnt++;
        ip->fat32_i.cluster_end = fat_new;
    }
    // TODO: off如果超出文件大小。
    // 这里先假设off不会超出文件大小。

    int init_s_n = LOGISTIC_S_NUM(off);
    int init_s_offset = LOGISTIC_S_OFFSET(off);

    ssize_t ret = 0;
    // read the target sector
    while ((!ISEOF(iter_n) && tot < n)) {
        int first_sector = FirstSectorofCluster(iter_n);
        for (int s = init_s_n; s < ip->i_sb->sectors_per_block && tot < n; s++) {
            bp = bread(ip->i_dev, first_sector + s);
            m = MIN(BSIZE - init_s_offset, n - tot);

            if (either_copyin(bp->data + init_s_offset, user_src, src, m) == -1) {
                brelse(bp);
                ret = -1;
                break;
            }
            bwrite(bp); // !!!!
            brelse(bp);
            tot += m;
            src += m;
            init_s_offset = 0;
        }
        if (tot == n || ret == -1)
            break;
        init_s_n = 0;
        next = fat32_next_cluster(iter_n);
        if (ISEOF(next)) {
            FAT_entry_t fat_new = fat32_cluster_alloc(ROOTDEV);
            fat32_fat_set(iter_n, fat_new);
            iter_n = fat_new;
            ip->fat32_i.cluster_cnt++;
            ip->fat32_i.cluster_end = fat_new;
        } else {
            iter_n = next;
        }
    }
    if (off + n > fileSize) {
        if (ip->i_type == T_FILE)
            ip->i_size = off + tot;
        else
            ip->i_size = CEIL_DIVIDE(off + tot, ip->i_sb->cluster_size) * (ip->i_sb->cluster_size);
    }
    fat32_inode_update(ip);
    if (ret == 0)
        return tot;
    else
        return ret;
}

struct inode *fat32_inode_dup(struct inode *ip) {
    acquire(&inode_table.lock);
    ip->ref++;
    release(&inode_table.lock);
    return ip;
}

// get a inode , move it from disk to memory
struct inode *fat32_inode_get(uint dev, uint inum, const char *name, uint parentoff) {
    struct inode *ip = NULL, *empty = NULL;
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
    ip->i_op = get_inodeops[FAT32]();
    ip->fs_type = FAT32;

    safestrcpy(ip->fat32_i.fname, name, strlen(name));

    release(&inode_table.lock);
    return ip;
}

// 获取fat32 inode的锁 并加载 磁盘中的short dirent to mem
void fat32_inode_lock(struct inode *ip) {
    struct buffer_head *bp;
    if (ip == 0 || ip->ref < 1)
        panic("inode lock");
    sema_wait(&ip->i_sem);

    if (ip->valid == 0) {
        uint sector_num = FATINUM_TO_SECTOR(ip->i_ino);
        uint sector_offset = FATINUM_TO_OFFSET(ip->i_ino);
        // uint sec_pos = DEBUG_SECTOR(ip, sector_num); // debug
        // printf("%d\n",sec_pos);
        bp = bread(ip->i_dev, sector_num);
        dirent_s_t *dirent_s_tmp = (dirent_s_t *)bp->data + sector_offset;

        ip->fat32_i.Attr = dirent_s_tmp->DIR_Attr;
        ip->fat32_i.cluster_start = DIR_FIRST_CLUS(dirent_s_tmp->DIR_FstClusHI, dirent_s_tmp->DIR_FstClusLO);
        if (ip->fat32_i.cluster_start == fat32_sb.fat32_sb_info.root_cluster_s && fat32_namecmp(ip->fat32_i.fname, "/")) {
            panic("this disk image has error \n");
        }
        // !!!
        if (ip->fat32_i.cluster_start != 0) {
            ip->fat32_i.cluster_cnt = fat32_fat_travel(ip, 0);
        } else {
            printfRed("the cluster_start of the file %s is zero\n ", ip->fat32_i.fname);
        }
        ip->fat32_i.DIR_CrtTimeTenth = dirent_s_tmp->DIR_CrtTimeTenth;

        ip->fat32_i.DIR_CrtTime = dirent_s_tmp->DIR_CrtTime;
        ip->fat32_i.DIR_CrtDate = dirent_s_tmp->DIR_CrtDate;
        ip->fat32_i.DIR_LstAccDate = dirent_s_tmp->DIR_LstAccDate;
        ip->fat32_i.DIR_WrtTime = dirent_s_tmp->DIR_WrtTime;
        ip->fat32_i.DIR_CrtDate = dirent_s_tmp->DIR_CrtDate;
        // memmove((void *)&ip->fat32_i.DIR_CrtTime, (void *)&dirent_s_tmp->DIR_CrtTime, sizeof(dirent_s_tmp->DIR_CrtTime));
        // memmove((void *)&ip->fat32_i.DIR_CrtDate, (void *)&dirent_s_tmp->DIR_CrtDate, sizeof(dirent_s_tmp->DIR_CrtDate));
        // memmove((void *)&ip->fat32_i.DIR_LstAccDate, (void *)&dirent_s_tmp->DIR_LstAccDate, sizeof(dirent_s_tmp->DIR_LstAccDate));
        // memmove((void *)&ip->fat32_i.DIR_WrtTime, (void *)&dirent_s_tmp->DIR_WrtTime, sizeof(dirent_s_tmp->DIR_WrtTime));
        // memmove((void *)&ip->fat32_i.DIR_WrtDate, (void *)&dirent_s_tmp->DIR_WrtDate, sizeof(dirent_s_tmp->DIR_WrtDate));
        ip->fat32_i.DIR_FileSize = dirent_s_tmp->DIR_FileSize;

        ip->i_rdev = dirent_s_tmp->DIR_Dev;

        if (ip->i_rdev)
            ip->i_type = T_DEVICE; // device?
        else if (DIR_BOOL(ip->fat32_i.Attr))
            ip->i_type = T_DIR;
        else
            ip->i_type = T_FILE;

        if (ip->i_type == T_FILE)
            ip->i_size = ip->fat32_i.DIR_FileSize;
        else if (ip->i_type == T_DIR)
            ip->i_size = DIRLENGTH(ip);
        else if (ip->i_type == T_DEVICE)
            ip->i_size = 0;

        ip->i_mode = READONLY_GET(dirent_s_tmp->DIR_Attr);

        // ip->i_op = get_fat32_iops();
        ip->i_op = get_inodeops[FAT32]();
        ip->fs_type = FAT32;
        // ip->i_sb = &fat32_sb;

        brelse(bp);
        ip->valid = 1;
        if (ip->fat32_i.Attr == 0)
            panic("fat32_inode_lock: no Attr");
    }
}

int fat32_inode_load_from_disk(struct inode *ip) {
    struct buffer_head *bp;
    if (ip->valid == 0) {
        uint sector_num = FATINUM_TO_SECTOR(ip->i_ino);
        uint sector_offset = FATINUM_TO_OFFSET(ip->i_ino);
        // uint sec_pos = DEBUG_SECTOR(ip, sector_num); // debug
        // printf("%d\n",sec_pos);
        bp = bread(ip->i_dev, sector_num);
        dirent_s_t *dirent_s_tmp = (dirent_s_t *)bp->data + sector_offset;

        ip->fat32_i.Attr = dirent_s_tmp->DIR_Attr;
        ip->fat32_i.cluster_start = DIR_FIRST_CLUS(dirent_s_tmp->DIR_FstClusHI, dirent_s_tmp->DIR_FstClusLO);
        if (ip->fat32_i.cluster_start == fat32_sb.fat32_sb_info.root_cluster_s && !fat32_namecmp(ip->fat32_i.fname, "/")) {
            return -1;
        }
        // !!!
        if (ip->fat32_i.cluster_start != 0) {
            ip->fat32_i.cluster_cnt = fat32_fat_travel(ip, 0);
        } else {
            printfRed("the cluster_start of the file %s is zero\n ", ip->fat32_i.fname);
        }
        ip->fat32_i.DIR_CrtTimeTenth = dirent_s_tmp->DIR_CrtTimeTenth;

        ip->fat32_i.DIR_CrtTime = dirent_s_tmp->DIR_CrtTime;
        ip->fat32_i.DIR_CrtDate = dirent_s_tmp->DIR_CrtDate;
        ip->fat32_i.DIR_LstAccDate = dirent_s_tmp->DIR_LstAccDate;
        ip->fat32_i.DIR_WrtTime = dirent_s_tmp->DIR_WrtTime;
        ip->fat32_i.DIR_CrtDate = dirent_s_tmp->DIR_CrtDate;
        // memmove((void *)&ip->fat32_i.DIR_CrtTime, (void *)&dirent_s_tmp->DIR_CrtTime, sizeof(dirent_s_tmp->DIR_CrtTime));
        // memmove((void *)&ip->fat32_i.DIR_CrtDate, (void *)&dirent_s_tmp->DIR_CrtDate, sizeof(dirent_s_tmp->DIR_CrtDate));
        // memmove((void *)&ip->fat32_i.DIR_LstAccDate, (void *)&dirent_s_tmp->DIR_LstAccDate, sizeof(dirent_s_tmp->DIR_LstAccDate));
        // memmove((void *)&ip->fat32_i.DIR_WrtTime, (void *)&dirent_s_tmp->DIR_WrtTime, sizeof(dirent_s_tmp->DIR_WrtTime));
        // memmove((void *)&ip->fat32_i.DIR_WrtDate, (void *)&dirent_s_tmp->DIR_WrtDate, sizeof(dirent_s_tmp->DIR_WrtDate));
        ip->fat32_i.DIR_FileSize = dirent_s_tmp->DIR_FileSize;

        ip->i_rdev = dirent_s_tmp->DIR_Dev;

        if (ip->i_rdev)
            ip->i_type = T_DEVICE; // device?
        else if (DIR_BOOL(ip->fat32_i.Attr))
            ip->i_type = T_DIR;
        else
            ip->i_type = T_FILE;

        if (ip->i_type == T_FILE)
            ip->i_size = ip->fat32_i.DIR_FileSize;
        else if (ip->i_type == T_DIR)
            ip->i_size = DIRLENGTH(ip);
        else if (ip->i_type == T_DEVICE)
            ip->i_size = 0;

        ip->i_mode = READONLY_GET(dirent_s_tmp->DIR_Attr);

        brelse(bp);
        ip->valid = 1;
        if (ip->fat32_i.Attr == 0)
            panic("fat32_inode_lock: no Attr");
    }
    return 1;
}

// 释放fat32 inode的锁
void fat32_inode_unlock(struct inode *ip) {
    // if (ip == 0 || !holdingsleep(&ip->i_sem) || ip->ref < 1)
    if (ip == 0 || ip->ref < 1)
        panic("fat32 unlock");
    sema_signal(&ip->i_sem);
}

// fat32 inode put : trunc and update
void fat32_inode_put(struct inode *ip) {
    acquire(&inode_table.lock);

    if (ip->ref == 1 && ip->valid && ip->i_nlink == 0) {
        sema_wait(&ip->i_sem);

        release(&inode_table.lock);
        fat32_inode_trunc(ip);
        ip->fat32_i.Attr = 0;
        fat32_inode_update(ip);
        ip->valid = 0;

        sema_signal(&ip->i_sem);

        acquire(&inode_table.lock);
    }

    ip->ref--;
    release(&inode_table.lock);
}

// unlock and put
void fat32_inode_unlock_put(struct inode *ip) {
    fat32_inode_unlock(ip);
    fat32_inode_put(ip);
}

// truncate the fat32 inode
void fat32_inode_trunc(struct inode *ip) {
    FAT_entry_t iter_n = ip->fat32_i.cluster_start;
    // FAT_entry_t end = 0;
    // uint32 FAT_sector_num;
    // uint32 FAT_sector_offset;

    // truncate the data
    while (!ISEOF(iter_n)) {
        FAT_entry_t fat_next = fat32_next_cluster(iter_n);
        fat32_fat_set(iter_n, EOC);
        iter_n = fat_next;
    }
    fat32_inode_delete(ip->parent, ip);
    ip->fat32_i.cluster_start = 0;
    ip->fat32_i.cluster_end = 0;
    ip->fat32_i.parent_off = 0;
    ip->fat32_i.cluster_cnt = 0;
    ip->fat32_i.DIR_FileSize = 0;
    ip->i_rdev = 0;
    ip->i_mode = IMODE_NONE;
    ip->i_size = 0;
    fat32_inode_update(ip);
}

// update
void fat32_inode_update(struct inode *ip) {
    struct buffer_head *bp;
    bp = bread(ip->i_dev, FATINUM_TO_SECTOR(ip->i_ino));
    dirent_s_t *dirent_s_tmp = (dirent_s_t *)bp->data + FATINUM_TO_OFFSET(ip->i_ino);

    // root has no fcb, so we don't update it!!!
    if (ip->i_ino == 0) {
        brelse(bp);
        return;
    }
    // uint sec_num = DEBUG_SECTOR(ip, FATINUM_TO_SECTOR(ip->i_ino)); // debug
    dirent_s_tmp->DIR_Attr = ip->fat32_i.Attr;
    dirent_s_tmp->DIR_LstAccDate = ip->fat32_i.DIR_LstAccDate;
    dirent_s_tmp->DIR_WrtDate = ip->fat32_i.DIR_WrtDate;
    dirent_s_tmp->DIR_WrtTime = ip->fat32_i.DIR_WrtTime;
    // memmove((void *)&dirent_s_tmp->DIR_LstAccDate, (void *)&ip->fat32_i.DIR_LstAccDate, sizeof(ip->fat32_i.DIR_LstAccDate));
    // memmove((void *)&dirent_s_tmp->DIR_WrtDate, (void *)&ip->fat32_i.DIR_WrtDate, sizeof(ip->fat32_i.DIR_WrtDate));
    // memmove((void *)&dirent_s_tmp->DIR_WrtTime, (void *)&ip->fat32_i.DIR_WrtTime, sizeof(ip->fat32_i.DIR_WrtTime));
    dirent_s_tmp->DIR_FstClusHI = DIR_FIRST_HIGH(ip->fat32_i.cluster_start);
    dirent_s_tmp->DIR_FstClusLO = DIR_FIRST_LOW(ip->fat32_i.cluster_start);
    // dirent_s_tmp->DIR_FileSize = ip->i_size;
    if (ip->i_type == T_DIR || ip->i_type == T_DEVICE) {
        dirent_s_tmp->DIR_FileSize = 0;
    } else {
        dirent_s_tmp->DIR_FileSize = ip->i_size;
    }

    dirent_s_tmp->DIR_Attr |= ip->i_mode;
    if (ip->i_type == T_DEVICE)
        dirent_s_tmp->DIR_Dev = ip->i_dev;
    else
        dirent_s_tmp->DIR_Dev = 0;

    bwrite(bp);
    brelse(bp);
}

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

ushort fat32_longname_popstack(Stack_t *fcb_stack, uchar *fcb_s_name, char *name_buf) {
    int name_idx = 0;
    ushort long_valid = 0;
    dirent_l_t fcb_l_tmp;
    if (stack_is_empty(fcb_stack)) {
        return 0;
    }
    uchar cnt = 1;
    // reverse the stack to check every long directory entry
    while (!stack_is_empty(fcb_stack)) {
        fcb_l_tmp = stack_pop(fcb_stack);
        if (!stack_is_empty(fcb_stack) && cnt != fcb_l_tmp.LDIR_Ord) {
            return 0;
        } else {
            cnt++;
        }
        uchar checksum = ChkSum(fcb_s_name);
        if (fcb_l_tmp.LDIR_Chksum != checksum) {
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
    long_valid = LAST_LONG_ENTRY_BOOL(fcb_l_tmp.LDIR_Ord) ? 1 : 0;
    return long_valid;
}

// checksum
uchar ChkSum(uchar *pFcbName) {
    uchar Sum = 0;
    for (short FcbNameLen = 11; FcbNameLen != 0; FcbNameLen--) {
        Sum = ((Sum & 1) ? 0x80 : 0) + (Sum >> 1) + *pFcbName++;
    }
    return Sum;
}

struct inode *fat32_inode_dirlookup(struct inode *ip, const char *name, uint *poff) {
    if (!DIR_BOOL((ip->fat32_i.Attr)))
        panic("dirlookup not DIR");
    struct buffer_head *bp;
    struct inode *ip_search;
    FAT_entry_t iter_n = ip->fat32_i.cluster_start;

    char name_buf[NAME_LONG_MAX];
    memset(name_buf, 0, sizeof(name_buf));
    Stack_t fcb_stack;
    stack_init(&fcb_stack);

    int first_sector;
    uint off = 0;
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
                // the first long directory in the data region
                if (NAME0_FREE_ALL(fcb_s[idx].DIR_Name[0])) {
                    brelse(bp);
                    stack_free(&fcb_stack);
                    return 0;
                }
                while (LONG_NAME_BOOL(fcb_l[idx].LDIR_Attr) && idx < FCB_PER_BLOCK) {
                    // printf("%d, %d, %d\n",LONG_NAME_BOOL(fcb_l[idx].LDIR_Attr), first_long_dir(ip), idx < FCB_PER_BLOCK);
                    stack_push(&fcb_stack, fcb_l[idx++]);
                    off++;
                }

                // pop stack
                if (!LONG_NAME_BOOL(fcb_l[idx].LDIR_Attr) && !NAME0_FREE_BOOL(fcb_s[idx].DIR_Name[0]) && idx < FCB_PER_BLOCK) {
                    memset(name_buf, 0, sizeof(name_buf));
                    ushort long_valid = fat32_longname_popstack(&fcb_stack, fcb_s[idx].DIR_Name, name_buf);

                    // if the long directory is invalid
                    if (!long_valid) {
                        fat32_short_name_parser(fcb_s[idx], name_buf);
                    }
                    //  search for?
                    if (fat32_namecmp(name, name_buf) == 0) {
                        // inode matches path element
                        if (poff)
                            *poff = off;
                        brelse(bp);

                        ip_search = fat32_inode_get(ip->i_dev, SECTOR_TO_FATINUM(first_sector + s, idx), name, off);
                        ip_search->parent = ip;
                        ip_search->i_nlink = 1;

                        stack_free(&fcb_stack);
                        return ip_search;
                    }
                }
                idx++;
                off++;
            }
            brelse(bp);
        }
        iter_n = fat32_next_cluster(iter_n);
    }
    stack_free(&fcb_stack);
    return 0;
}

// direntory link (hard link)
int fat32_inode_dirlink(struct inode *dp, char *name) {
    // int off;
    struct inode *ip;

    // Check that name is not present.
    if ((ip = fat32_inode_dirlookup(dp, name, 0)) != 0) {
        fat32_inode_put(ip);
        return -1;
    }
    ip->i_nlink++;
    fat32_inode_put(ip);
    return 0;
}

// return a pointer to a new inode with lock on
// no need to  lock dp before call this func
struct inode *fat32_inode_create(struct inode *dp, const char *name, uchar type, short major, short minor) {
    struct inode *ip = NULL;

    ASSERT(dp);
    fat32_inode_lock(dp);
    // have existed?
    if ((ip = fat32_inode_dirlookup(dp, name, 0)) != 0) {
        fat32_inode_unlock_put(dp);
        fat32_inode_lock(ip);

        // fat32_inode_load_from_disk(ip);
        //  if (type == T_FILE && (ip->i_type == T_FILE || ip->i_type == T_DEVICE))
        //      return ip;
        if (type == ip->i_type) {
            return ip;
        }
        fat32_inode_unlock_put(ip);
        return 0;
    }

    // haven't exited
    if ((ip = fat32_inode_alloc(dp, name, type)) == 0) {
        fat32_inode_unlock_put(dp);
        return 0;
    }
    // #ifdef __DEBUG_FS__
    //     if (type == T_FILE)
    //         printfRed("create : create file, %s\n", path);
    //     else if (type == T_DEVICE)
    //         printfRed("create : create device, %s\n", path);
    //     else if (type == T_DIR)
    //         printfRed("create : create directory, %s\n", path);
    // #endif
    fat32_inode_lock(ip);
    // fat32_inode_load_from_disk(ip);

    ip->i_nlink = 1;
    ip->i_rdev = mkrdev(major, minor);
    ip->i_type = type;
    fat32_inode_update(ip);

    if (type == T_DIR) { // Create . and .. entries.
        // // No ip->nlink++ for ".": avoid cyclic ref count.
        // if (fat32_inode_dirlink(ip, ".") < 0 || fat32_inode_dirlink(ip, "..") < 0)
        //     goto fail;
        // TODO : dirlink

        // direntory . and .. , write them to the disk
        uchar fcb_dot_char[64];
        memset(fcb_dot_char, 0, sizeof(fcb_dot_char));
        fat32_fcb_init(ip, (const uchar *)".", ATTR_DIRECTORY, (char *)fcb_dot_char);
        uint tot = fat32_inode_write(ip, 0, (uint64)fcb_dot_char, 0, 32);
        ASSERT(tot == 32);

        uchar fcb_dotdot_char[64];
        memset(fcb_dotdot_char, 0, sizeof(fcb_dotdot_char));
        fat32_fcb_init(ip, (const uchar *)"..", ATTR_DIRECTORY, (char *)fcb_dotdot_char);
        tot = fat32_inode_write(ip, 0, (uint64)fcb_dotdot_char, 32, 32);
        ASSERT(tot == 32);
    }

    // if (fat32_inode_dirlink(dp, name) < 0)
    //     goto fail;

    if (type == T_DIR) {
        // now that success is guaranteed:
        fat32_inode_update(dp);
    }

    fat32_inode_unlock_put(dp); // !!! bug

    return ip;
}

// allocate a new inode
struct inode *fat32_inode_alloc(struct inode *dp, const char *name, uchar type) {
    uchar attr;
    if (type == T_DIR)
        DIR_SET(attr);
    else
        NONE_DIR_SET(attr);
    uchar fcb_char[FCB_MAX_LENGTH];
    memset(fcb_char, 0, sizeof(fcb_char));
    int fcb_cnt = fat32_fcb_init(dp, (const uchar *)name, attr, (char *)fcb_char);
    // uint sector_pos = FirstSectorofCluster(dp->fat32_i.cluster_start) * 512; // debug
    uint offset = fat32_dir_fcb_insert_offset(dp, fcb_cnt);
    uint tot = fat32_inode_write(dp, 0, (uint64)fcb_char, offset * sizeof(dirent_l_t), fcb_cnt * sizeof(dirent_l_t));

    ASSERT(tot == fcb_cnt * sizeof(dirent_l_t));

    struct inode *ip_new;

    uint fcb_new_off = (offset + fcb_cnt - 1) * sizeof(dirent_s_t);
    uint C_NUM = LOGISTIC_C_NUM(fcb_new_off) + 1;
    // find the target cluster of off
    uint target_cluster = fat32_fat_travel(dp, C_NUM);
    uint first_sector = FirstSectorofCluster(target_cluster);

    uint C_OFFSET = LOGISTIC_C_OFFSET(fcb_new_off);
    uint sector_num = first_sector + C_OFFSET / fat32_sb.sector_size;
    uint sector_offset = (C_OFFSET % fat32_sb.sector_size) / sizeof(dirent_l_t);
    uint fat_num = SECTOR_TO_FATINUM(sector_num, sector_offset);

    // // first_sector = FirstSectorofCluster(target_cluster); // debug
    // sector_num = first_sector + C_OFFSET / FCB_PER_BLOCK;
    // uint sec_pos = DEBUG_SECTOR(dp, sector_num); // debug
    // printf("%d\n",sec_pos); // debug

    ip_new = fat32_inode_get(dp->i_dev, fat_num, name, offset + fcb_cnt - 1);
    ip_new->i_nlink = 1;
    ip_new->ref = 1;
    ip_new->parent = dp;
    ip_new->i_type = type;

    ip_new->i_op = get_inodeops[FAT32]();
    ip_new->fs_type = FAT32;
    // ip_new->i_sb = &fat32_sb;

    // uint fat_num_dp = SECTOR_TO_FATINUM(first_sector, 0); // debug
    // uint sector_pos = FirstSectorofCluster(ip_new->fat32_i.cluster_start) * 512; // debug
    return ip_new;
}

// fcb init
// 为 long_name 初始化若干个 dirent_l_t 和一个dirent_s_t，写入 fcb_char中，返回需要的 fcb 总数
int fat32_fcb_init(struct inode *ip_parent, const uchar *long_name, uchar attr, char *fcb_char) {
    dirent_s_t dirent_s_cur;
    memset((void *)&dirent_s_cur, 0, sizeof(dirent_s_cur));
    // dirent_l_t dirent_l_cur;
    dirent_s_cur.DIR_Attr = attr;

    uint long_idx = -1;

    // . and ..
    if (!fat32_namecmp((const char *)long_name, ".") || !fat32_namecmp((const char *)long_name, "..")) {
        strncpy((char *)dirent_s_cur.DIR_Name, (const char *)long_name, strlen((const char *)long_name));
        dirent_s_cur.DIR_FileSize = 0;
        dirent_s_cur.DIR_FstClusHI = DIR_FIRST_HIGH(ip_parent->fat32_i.cluster_start);
        dirent_s_cur.DIR_FstClusLO = DIR_FIRST_LOW(ip_parent->fat32_i.cluster_start);
        memmove(fcb_char, &dirent_s_cur, sizeof(dirent_s_cur));
        return 1;
    }

    int ret_cnt = 0;
    char file_name[NAME_LONG_MAX];
    char file_ext[4];
    /*short dirent*/
    int name_len = strlen((const char *)long_name);
    // 数据文件
    if (!DIR_BOOL(attr)) {
        if (str_split((const char *)long_name, '.', file_name, file_ext) == -1) {
            // printfRed("it is a file without extname\n");
            strncpy(file_name, (char *)long_name, 8);
            dirent_s_cur.DIR_Name[8] = 0x20;
            dirent_s_cur.DIR_Name[9] = 0x20;
            dirent_s_cur.DIR_Name[10] = 0x20;
        } else {
            str_toupper(file_ext);
            strncpy((char *)dirent_s_cur.DIR_Name + 8, file_ext, 3); // extend name
            if (strlen(file_ext) == 2) {
                dirent_s_cur.DIR_Name[10] = 0x20;
            }
            if (strlen(file_ext) == 1) {
                dirent_s_cur.DIR_Name[10] = 0x20;
                dirent_s_cur.DIR_Name[9] = 0x20;
            }
        }

        str_toupper(file_name);
        strncpy((char *)dirent_s_cur.DIR_Name, (char *)file_name, 8);
        if (strlen((char *)long_name) > 8) {
            long_idx = fat32_find_same_name_cnt(ip_parent, file_name);
            dirent_s_cur.DIR_Name[6] = '~';
            dirent_s_cur.DIR_Name[7] = long_idx + 0x31;
        }
    } else {
        // 目录文件
        strncpy(file_name, (char *)long_name, 11);

        str_toupper(file_name);
        strncpy((char *)dirent_s_cur.DIR_Name, file_name, 8);
        if (strlen((const char *)long_name) > 8) {
            long_idx = fat32_find_same_name_cnt(ip_parent, file_name);

            // strncpy(dirent_s_cur.DIR_Name + 8, file_name + (name_len - 3), 3); // last three char
            dirent_s_cur.DIR_Name[6] = '~';
            dirent_s_cur.DIR_Name[7] = long_idx + 0x31;
            memset(dirent_s_cur.DIR_Name + 8, 0x20, 3);
        }
    }
    dirent_s_cur.DIR_FileSize = 0;

    uint first_c = fat32_cluster_alloc(ip_parent->i_dev);
    dirent_s_cur.DIR_FstClusHI = DIR_FIRST_HIGH(first_c);
    dirent_s_cur.DIR_FstClusLO = DIR_FIRST_LOW(first_c);

    /*push long dirent into stack*/
    Stack_t fcb_stack;
    stack_init(&fcb_stack);
    // int iter_order = 1;      // the compiler says it's an unused variable
    uchar checksum = ChkSum(dirent_s_cur.DIR_Name);
#ifdef __DEBUG_FS__
    printf("inode init : pid %d, %s, checksum = %x \n", proc_current()->pid, long_name, checksum);
#endif
    int char_idx = 0;
    // every long name entry
    int ord_max = CEIL_DIVIDE(name_len, FAT_LFN_LENGTH);
    for (int i = 1; i <= ord_max; i++) {
        dirent_l_t dirent_l_cur;
        memset((void *)&(dirent_l_cur), 0xFF, sizeof(dirent_l_cur));
        if (char_idx == name_len)
            break;

        // order
        if (i == ord_max)
            dirent_l_cur.LDIR_Ord = LAST_LONG_ENTRY_SET(i);
        else
            dirent_l_cur.LDIR_Ord = i;

        // Name
        int end_flag = 0;
        for (int i = 0; i < 5 && !end_flag; i++) {
            dirent_l_cur.LDIR_Name1[i] = LONG_NAME_CHAR_SET(long_name[char_idx]);
            if (!LONG_NAME_CHAR_VALID(long_name[char_idx]))
                end_flag = 1;
            char_idx++;
        }
        for (int i = 0; i < 6 && !end_flag; i++) {
            dirent_l_cur.LDIR_Name2[i] = LONG_NAME_CHAR_SET(long_name[char_idx]);
            if (!LONG_NAME_CHAR_VALID(long_name[char_idx]))
                end_flag = 1;
            char_idx++;
        }
        for (int i = 0; i < 2 && !end_flag; i++) {
            dirent_l_cur.LDIR_Name3[i] = LONG_NAME_CHAR_SET(long_name[char_idx]);
            if (!LONG_NAME_CHAR_VALID(long_name[char_idx]))
                end_flag = 1;
            char_idx++;
        }

        // Attr  and  Type
        dirent_l_cur.LDIR_Attr = ATTR_LONG_NAME;
        dirent_l_cur.LDIR_Type = 0;

        // check sum
        dirent_l_cur.LDIR_Chksum = checksum;

        // must set to zero
        dirent_l_cur.LDIR_Nlinks = 0;
        stack_push(&fcb_stack, dirent_l_cur);
    }

    dirent_l_t fcb_l_tmp;
    while (!stack_is_empty(&fcb_stack)) {
        fcb_l_tmp = stack_pop(&fcb_stack);
        memmove(fcb_char, &fcb_l_tmp, sizeof(fcb_l_tmp));
        fcb_char = fcb_char + sizeof(fcb_l_tmp);
        ret_cnt++;
    }
    // the first long dirent
    fcb_l_tmp.LDIR_Nlinks = 1;
    memmove(fcb_char, &dirent_s_cur, sizeof(dirent_s_cur));
    stack_free(&fcb_stack);
    return ret_cnt + 1;
    // TODO: 获取当前时间和日期，还有TimeTenth
}

// find the same prefix and same extend name !!!
uint fat32_find_same_name_cnt(struct inode *ip, char *name) {
    if (!DIR_BOOL((ip->fat32_i.Attr)))
        panic("find same name cnt not DIR");

    struct buffer_head *bp;
    FAT_entry_t iter_n = ip->fat32_i.cluster_start;

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
                if (!LONG_NAME_BOOL(fcb_s[idx].DIR_Attr) && !NAME0_FREE_BOOL(fcb_s[idx].DIR_Name[0])) {
                    // is our search for?
                    // extend name should be matched!!!
                    if (!strncmp((char *)fcb_s[idx].DIR_Name, name, 6) && fcb_s[idx].DIR_Name[6] == '~') {
                        ret++;
                    }
                }
                if (NAME0_FREE_ALL(fcb_s[idx].DIR_Name[0])) {
                    brelse(bp);
                    return ret;
                }
                idx++;
            }
            brelse(bp);
        }
        iter_n = fat32_next_cluster(iter_n);
    }
    return ret;
}

// 获取fcb的插入位置(可以插入到碎片中)
// 在目录节点中找到能插入 fcb_cnt_req 个 fcb 的启始偏移位置，并返回它
uint fat32_dir_fcb_insert_offset(struct inode *ip, uchar fcb_cnt_req) {
    struct buffer_head *bp;
    FAT_entry_t iter_n = ip->fat32_i.cluster_start;
    // uint32 FAT_sec_no;   // the compiler says it's an unused variable
    uint first_sector;

    uchar fcb_free_cnt = 0;
    uint offset_ret_base = DIRLENGTH(ip) / sizeof(dirent_l_t);
    uint offset_ret_final = 0; // 为通过编译给一个初始值

    uint idx = 0;
    while (!ISEOF(iter_n)) {
        first_sector = FirstSectorofCluster(iter_n);
        for (int s = 0; s < ip->i_sb->sectors_per_block; s++) {
            bp = bread(ip->i_dev, first_sector + s);
            dirent_s_t *diernt_s_tmp = (dirent_s_t *)(bp->data);
            for (int i = 0; i < FCB_PER_BLOCK; i++, idx++) {
                if (!NAME0_FREE_BOOL(diernt_s_tmp[i].DIR_Name[0])) {
                    fcb_free_cnt = 0;
                    offset_ret_final = offset_ret_base;
                } else {
                    if (fcb_free_cnt == 0) {
                        offset_ret_final = idx;
                    }
                    fcb_free_cnt++;
                    if (fcb_free_cnt == fcb_cnt_req) {
                        brelse(bp);
                        return offset_ret_final;
                    }
                }
            }
            brelse(bp);
        }
        iter_n = fat32_next_cluster(iter_n);
    }
    return offset_ret_final;
}

inline int fat32_isdir(struct inode *ip) {
    return DIR_BOOL(ip->fat32_i.Attr);
}

// 一个目录除了 .. 和 . 是否为空？
int fat32_isdirempty(struct inode *ip) {
    struct buffer_head *bp;
    FAT_entry_t iter_n = ip->fat32_i.cluster_start;
    uint first_sector;

    while (!ISEOF(iter_n)) {
        first_sector = FirstSectorofCluster(iter_n);

        for (int s = 0; s < ip->i_sb->sectors_per_block; s++) {
            bp = bread(ip->i_dev, first_sector + s);
            dirent_s_t *diernt_s_tmp = (dirent_s_t *)(bp->data);
            for (int i = 0; i < FCB_PER_BLOCK; i++) {
                // not free
                if (!NAME0_FREE_BOOL(diernt_s_tmp[i].DIR_Name[0])) {
                    if (fat32_namecmp((char *)diernt_s_tmp[i].DIR_Name, ".") && fat32_namecmp((char *)diernt_s_tmp[i].DIR_Name, "..")) {
                        brelse(bp); // !!!!
                        return 0;
                    }
                }
            }
            brelse(bp);
        }
        iter_n = fat32_next_cluster(iter_n);
    }
    return 1;
}

// get time string
// int fat32_time_parser(uint16 *time_in, char *str, int ms) {
//     uint CreateTimeMillisecond;
//     uint TimeSecond = time_in->second_per_2 << 1;
//     uint TimeMinute = time_in->minute;
//     uint TimeHour = time_in->hour;

//     if (ms) {
//         CreateTimeMillisecond = (uint)(ms)*10; // 计算毫秒数
//         sprintf(str, "%d:%02d:%02d.%03d", TimeHour, TimeMinute, TimeSecond, CreateTimeMillisecond);
//     } else {
//         sprintf(str, "%d:%02d:%02d", TimeHour, TimeMinute, TimeSecond);
//     }

//     return 1;
// }

// get date string
// int fat32_date_parser(uint16 *date_in, char *str) {
//     uint TimeDay = date_in->day;
//     uint TimeMonth = date_in->month;
//     uint TimeYear = date_in->year + 1980;

//     sprintf(str, "%04d-%02d-%02d", TimeYear, TimeMonth, TimeDay);

//     return 1;
// }

void fat32_inode_stati(struct inode *ip, struct kstat *st) {
    ASSERT(ip && st);
    st->st_atime_sec = ip->i_atime;
    st->st_atime_nsec = ip->i_atime * 1000000000;
    st->st_blksize = ip->i_blksize;
    st->st_blocks = ip->i_blocks;
    st->st_ctime_sec = ip->i_ctime;
    st->st_ctime_nsec = ip->i_ctime * 1000000000;
    st->st_dev = ip->i_dev;
    st->st_gid = ip->i_gid;
    st->st_ino = ip->i_ino;
    st->st_mode = ip->i_mode;
    st->st_mtime_sec = ip->i_mtime;
    st->st_mtime_nsec = ip->i_mtime * 1000000000;
    st->st_nlink = ip->i_nlink;
    st->st_rdev = ip->i_rdev;
    st->st_size = ip->i_size;
    st->st_uid = ip->i_uid;
    return;
}

// delete inode given its parent dp
int fat32_inode_delete(struct inode *dp, struct inode *ip) {
    int str_len = strlen(ip->fat32_i.fname);
    int off = ip->fat32_i.parent_off;
    ASSERT(off > 0);
    int long_dir_len = CEIL_DIVIDE(str_len, FAT_LFN_LENGTH); // 上取整
    char fcb_char[FCB_MAX_LENGTH];
    memset(fcb_char, 0, sizeof(fcb_char));
    for (int i = 0; i < long_dir_len + 1; i++)
        fcb_char[i * 32] = 0xE5;
    ASSERT(off - long_dir_len > 0);
#ifdef __DEBUG_FS__
    printf("inode delete : pid %d, %s, off = %d, long_dir_len = %d\n", proc_current()->pid, ip->fat32_i.fname, off, long_dir_len);
#endif
    uint tot = fat32_inode_write(dp, 0, (uint64)fcb_char, (off - long_dir_len) * sizeof(dirent_s_t), (long_dir_len + 1) * sizeof(dirent_s_t));
    ASSERT(tot == (long_dir_len + 1) * sizeof(dirent_l_t));
    return 0;
}

// zero cluster
void fat32_zero_cluster(uint64 c_num) {
    struct buffer_head *bp;
    int first_sector;
    first_sector = FirstSectorofCluster(c_num);
    for (int s = 0; s < (fat32_sb.sectors_per_block); s++) {
        bp = bread(fat32_sb.s_dev, first_sector + s);
        memset(bp->data, 0, BSIZE);
        bwrite(bp);
        brelse(bp);
    }
    return;
}

void fat32_short_name_parser(dirent_s_t dirent_s, char *name_buf) {
    int len_name = 0;
    for (int i = 0; i < 8; i++) {
        if (dirent_s.DIR_Name[i] != 0x20) {
            name_buf[len_name++] = dirent_s.DIR_Name[i];
        }
    }
    if (dirent_s.DIR_Name[8] != 0x20) {
        name_buf[len_name++] = '.';
        for (int i = 8; i < 11; i++) {
            if (dirent_s.DIR_Name[i] != 0x20) {
                name_buf[len_name++] = dirent_s.DIR_Name[i];
            }
        }
    }
    name_buf[len_name] = '\0';
}

// get time string
// int fat32_time_parser(uint16 *time_in, char *str, int ms) {
//     uint CreateTimeMillisecond;
//     uint TimeSecond = time_in->second_per_2 << 1;
//     uint TimeMinute = time_in->minute;
//     uint TimeHour = time_in->hour;

//     if (ms) {
//         CreateTimeMillisecond = (uint)(ms)*10; // 计算毫秒数
//         sprintf(str, "%d:%02d:%02d.%03d", TimeHour, TimeMinute, TimeSecond, CreateTimeMillisecond);
//     } else {
//         sprintf(str, "%d:%02d:%02d", TimeHour, TimeMinute, TimeSecond);
//     }

//     return 1;
// }

// get date string
// int fat32_date_parser(uint16 *date_in, char *str) {
//     uint TimeDay = date_in->day;
//     uint TimeMonth = date_in->month;
//     uint TimeYear = date_in->year + 1980;

//     sprintf(str, "%04d-%02d-%02d", TimeYear, TimeMonth, TimeDay);

//     return 1;
// }

// get time now
// uint16 fat32_inode_get_time(int *ms) {
//     // TODO
//     // uint16 time_ret;
//     // uint64 count;

//     // asm volatile("rdtime %0" : "=r"(count));
//     // // second and its reminder
//     // uint64 tick_per_second = 10000000;   // 时钟频率为 32.768 kHz
//     // uint64 seconds = count / tick_per_second;
//     // uint64 remainder = count % tick_per_second;

//     // // hour minute second
//     // uint64 total_seconds = (uint32)seconds;
//     // uint64 sec_per_hour = 3600;
//     // uint64 sec_per_minute = 60;

//     // time_ret.hour = total_seconds / sec_per_hour;
//     // time_ret.minute = (total_seconds / sec_per_minute) % 60;
//     // time_ret.second_per_2 = (total_seconds % 60)>>1;
//     // if(ms)
//     // {
//     //     // million second
//     //     uint64 tick_per_ms = tick_per_second / 1000;
//     //     *ms = remainder / tick_per_ms;
//     // }
//     // return time_ret;

//     return TODO();
// }

// get date now
// uint16 fat32_inode_get_date() {
//     // TODO
// }
