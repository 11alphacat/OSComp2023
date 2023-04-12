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
// static char *skepelem(char *path, char *name) {
//     char *s;
//     int len;

//     while (*path == '/')
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

// static fat_entry_t *fat32_fat_entry_namex(char *path, int nameeparent, char *name) {
//     fat_entry_t *fat_ep, *next;

//     if (*path == '/')
//         fat_ep = fat32_fat_entry_dup(global_fatfs.root_entry);
//     else
//         fat_ep = fat32_fat_entry_dup(myproc()->fat_cwd);

//     // while ((path = skepelem(path, name)) != 0) {
//     //     ilock(fat_ep);
//     //     if (!DIR_BOOL(fat_ep->Attr)) {
//     //         iunlockput(fat_ep);
//     //         return 0;
//     //     }
//     //     if (nameeparent && *path == '\0') {
//     //         // Stop one level early.
//     //         iunlock(fat_ep);
//     //         return fat_ep;
//     //     }
//     //     if ((next = dirlookup(fat_ep, name, 0)) == 0) {
//     //         iunlockput(fat_ep);
//     //         return 0;
//     //     }
//     //     iunlockput(fat_ep);
//     //     fat_ep = next;
//     // }
//     // if (nameeparent) {
//     //     eput(fat_ep);
//     //     return 0;
//     // }
//     return fat_ep;
// }

// fat_entry_t *fat32_name_fat_entry(char *path) {
//     char name[PATH_LONG_MAX]; // 260
//     return fat32_fat_entry_namex(path, 0, name);
// }

// fat_entry_t *fat32_name_fat_entry_parent(char *path, char *name) {
//     return fat32_fat_entry_namex(path, 1, name);
// }

// fat_entry_t *fat32_fat_entry_dup(fat_entry_t *fat_ep) {
//     acquire(&fat_entry_table.lock);
//     fat_ep->ref++;
//     release(&fat_entry_table.lock);
//     return fat_ep;
// }

// void fat32_fat_entry_update(fat_entry_t *fat_ep) {
//     struct buffer_head *bp;
//     struct dinode *dip;

//     // bp = bread(fat_ep->dev, IBLOCK(fat_ep->inum, sb));
//     // dip = (struct dinode *)bp->data + fat_ep->inum % IPB;
//     // dip->type = fat_ep->type;
//     // dip->major = fat_ep->major;
//     // dip->minor = fat_ep->minor;
//     // dip->nlink = fat_ep->nlink;
//     // dip->size = fat_ep->size;
//     // memmove(dip->addrs, fat_ep->addrs, sizeof(fat_ep->addrs));
//     // bwrite(bp);
//     // brelse(bp);
// }

// void fat32_fat_entry_trunc(fat_entry_t *fat_ep) {
//     struct buffer_head *bp;

//     FAT_term_t iter_n = fat_ep->cluster_start;
//     uint32 FAT_s_n;
//     uint32 FAT_s_offset;
//     FAT_term_t end = 0;

//     while (!ISEOF(iter_n)) {
//         FAT_s_n = ThisFATSecNum(iter_n);
//         FAT_s_offset = ThisFATEntOffset(iter_n);
//         bp = bread(fat_ep->fatfs_obj->dev, FAT_s_n);
//         FAT_term_t fat_next = FAT32ClusEntryVal(bp->data, iter_n);
//         SetFAT32ClusEntryVal(bp->data, iter_n, EOC_MASK);
//         Show_bytes((byte_pointer)&bp->data, sizeof(bp->data));
//         bwrite(bp);
//         brelse(bp);
//         iter_n = fat_next;
//     }
//     fat_ep->cluster_start = -1;
//     fat_ep->cluster_end = -1;
//     fat_ep->parent_off = -1;
//     fat_ep->cluster_cnt = -1;
//     fat_ep->DIR_FileSize = 0;
//     fat32_fat_entry_update(fat_ep);
// }

// // 获取fat_entry的锁
// void fat32_fat_entry_lock(fat_entry_t *fat_ep) {
//     if (fat_ep == 0 || fat_ep->ref < 1)
//         panic("ilock");
//     acquiresleep(&fat_ep->lock);
// }

// // 释放fat_entry的锁
// void fat32_fat_entry_unlock(fat_entry_t *fat_ep) {
//     if (fat_ep == 0 || !holdingsleep(&fat_ep->lock) || fat_ep->ref < 1)
//         panic("iunlock");
//     releasesleep(&fat_ep->lock);
// }

// // fat_entry put : trunc and update
// void fat32_fat_entry_put(fat_entry_t *fat_ep) {
//     acquire(&fat_entry_table.lock);
//     if (fat_ep->ref == 1 && fat_ep->valid && fat_ep->nlink == 0) {
//         acquiresleep(&fat_ep->lock);

//         release(&fat_entry_table.lock);
//         fat32_fat_entry_trunc(fat_ep);
//         fat_ep->Attr = 0;
//         fat32_fat_entry_update(fat_ep);
//         fat_ep->valid = 0;

//         releasesleep(&fat_ep->lock);

//         acquire(&fat_entry_table.lock);
//     }

//     fat_ep->ref--;
//     release(&fat_entry_table.lock);
// }

// // unlock and put
// void fat32_fat_entry_unlock_put(fat_entry_t *fat_ep) {
//     fat32_fat_entry_unlock(fat_ep);
//     fat32_fat_entry_put(fat_ep);
// }

// #define fat32_namecmp(s, t) (strncmp(s, t, PATH_LONG_MAX))

// // for (off = 0; off < dp->DIR_FileSize; off += sizeof(de)) {
// //     if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
// //         panic("dirlookup read");
// //     if (de.inum == 0)
// //         continue;
// //     if (namecmp(name, de.name) == 0) {
// //         // entry matches path element
// //         if (poff)
// //             *poff = off;
// //         inum = de.inum;
// //         return iget(dp->dev, inum);
// //     }
// // }
// fat_entry_t *fat32_fat_entry_dirlookup(fat_entry_t *fat_ep, char *name, uint *poff) {
//     uint off, inum;

//     if (DIR_BOOL((fat_ep->Attr)))
//         panic("dirlookup not DIR");

//     return 0;
// }

// fat_entry_t *fat32_fat_entry_get(uint dev, dirent_s_t *dirent_s_t) {
//     fat_entry_t *fat_ep, *empty;
//     acquire(&fat_entry_table.lock);

//     // // Is the inode already in the table?
//     // empty = 0;
//     // for (ip = &itable.inode[0]; ip < &itable.inode[NINODE]; ip++) {
//     //     if (ip->ref > 0 && ip->dev == dev && ip->inum == inum) {
//     //         ip->ref++;
//     //         release(&itable.lock);
//     //         return ip;
//     //     }
//     //     if (empty == 0 && ip->ref == 0) // Remember empty slot.
//     //         empty = ip;
//     // }

//     // // Recycle an inode entry.
//     // if (empty == 0)
//     //     panic("iget: no inodes");

//     // ip = empty;
//     // ip->dev = dev;
//     // ip->inum = inum;
//     // ip->ref = 1;
//     // ip->valid = 0;
//     release(&fat_entry_table.lock);

//     return fat_ep;
// }

// // Read data from fat_entry.
// int fat32_fat_entry_read(fat_entry_t *ip, int user_dst, uint64 dst, uint off, uint n) {
//     uint tot, m;
//     struct buffer_head *bp;

//     // if (off > ip->size || off + n < off)
//     //     return 0;
//     // if (off + n > ip->size)
//     //     n = ip->size - off;

//     // for (tot = 0; tot < n; tot += m, off += m, dst += m) {
//     //     uint addr = bmap(ip, off / BSIZE);
//     //     if (addr == 0)
//     //         break;
//     //     bp = bread(ip->dev, addr);
//     //     m = min(n - tot, BSIZE - off % BSIZE);
//     //     if (either_copyout(user_dst, dst, bp->data + (off % BSIZE), m) == -1) {
//     //         brelse(bp);
//     //         tot = -1;
//     //         break;
//     //     }
//     //     brelse(bp);
//     // }
//     return tot;
// }

// // Write data to fat_entry
// int fat32_fat_entry_write(fat_entry_t *ip, int user_src, uint64 src, uint off, uint n) {
//     uint tot, m;
//     struct buffer_head *bp;

//     // if (off > ip->size || off + n < off)
//     //     return -1;
//     // if (off + n > MAXFILE * BSIZE)
//     //     return -1;

//     // for (tot = 0; tot < n; tot += m, off += m, src += m) {
//     //     uint addr = bmap(ip, off / BSIZE);
//     //     if (addr == 0)
//     //         break;
//     //     bp = bread(ip->dev, addr);
//     //     m = min(n - tot, BSIZE - off % BSIZE);
//     //     if (either_copyin(bp->data + (off % BSIZE), user_src, src, m) == -1) {
//     //         brelse(bp);
//     //         break;
//     //     }
//     //     log_write(bp);
//     //     brelse(bp);
//     // }

//     // if (off > ip->size)
//     //     ip->size = off;

//     // // write the i-node back to disk even if the size didn't change
//     // // because the loop above might have called bmap() and added a new
//     // // block to ip->addrs[].
//     // iupdate(ip);

//     return tot;
// }

// // void fat32_fat_entry_stat(fat_entry_t* fat_ep, struct stat *st) {
// //     st->dev = fat_ep->fatfs_obj->dev;
// //     // st->ino = fat_ep->inum;
// //     st->type = fat_ep->Attr;
// //     st->nlink = fat_ep->nlink;
// //     st->size = fat_ep->DIR_FileSize;
// // }

// int fat32_fat_dirlink(fat_entry_t *fat_ep, char *name, uint inum) {
//     // int off;
//     // struct dirent de;
//     // struct inode *ip;

//     // // Check that name is not present.
//     // if ((ip = dirlookup(dp, name, 0)) != 0) {
//     //     iput(ip);
//     //     return -1;
//     // }

//     // // Look for an empty dirent.
//     // for (off = 0; off < dp->size; off += sizeof(de)) {
//     //     if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
//     //         panic("dirlink read");
//     //     if (de.inum == 0)
//     //         break;
//     // }

//     // strncpy(de.name, name, DIRSIZ);
//     // de.inum = inum;
//     // if (writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
//     //     return -1;

//     return 0;
// }