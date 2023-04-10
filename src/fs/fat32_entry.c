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


extern FATFS_t global_fatfs;

// maximum file entry in memory
#define NENTRY 10
struct fat_entry_table_t{
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

uint32 fat32_fat_travel(uint32 s, uint32* e)
{
    struct buffer_head *bp;
    
    FAT_term_t iter_n = s;

    uint32 FAT_s_n;
    uint32 FAT_s_offset;
    int cnt=0;
    while(!ISEOF(iter_n))
    {
        cnt++;
        FAT_s_n =  ThisFATSecNum(iter_n);
        FAT_s_offset = ThisFATEntOffset(iter_n);

        bp = bread(ROOTDEV, FAT_s_n);
        FAT_term_t fat_next=FAT32ClusEntryVal(bp->data,iter_n);

        iter_n=fat_next;
        if(!ISEOF(iter_n))
            *e=iter_n;
    }
    // brelse(bp);
    return cnt;
}

fat_entry_t *fat32_root_entry_init(FATFS_t *fatfs) {
    /*先用kalloc分配一个物理块*/
    fat_entry_t *root = (fat_entry_t *)kalloc();
    /*初始化root_entry的一些字段*/
    initsleeplock(&root->lock, "fat_entry_root");
    root->parent = root; // 根目录父亲节点是自身
    root->fname[0] = '/';
    root->fname[1] = '\0';
    root->nlink = 1;
    root->ref = 1;
    root->valid = 1;
    root->cluster_start = 2;

    root->cluster_cnt = fat32_fat_travel(root->cluster_start, &root->cluster_end);
    root->parent_off = 0;
    DIR_SET(root->Attr);
    
    ASSERT(root->cluster_start == 2);
    root->fatfs_obj = fatfs;
    
    root->DIR_FileSize=0;
    // TODO: 设置i_mapping
    return root;
}

// Examples:
//   skepelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skepelem("///a//bb", name) = "bb", setting name = "a"
//   skepelem("a", name) = "", setting name = "a"
//   skepelem("", name) = skepelem("////", name) = 0
static char * skepelem(char *path, char *name) {
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

static fat_entry_t* namex(char *path, int nameeparent, char *name) {
    fat_entry_t* fat_ep, *next;

    if (*path == '/')
        fat_ep = fat32_fat_entry_dup(global_fatfs.root_entry);
    else
        fat_ep = fat32_fat_entry_dup(myproc()->fat_cwd);

    // while ((path = skepelem(path, name)) != 0) {
    //     ilock(fat_ep);
    //     if (!DIR_BOOL(fat_ep->Attr)) {
    //         iunlockput(fat_ep);
    //         return 0;
    //     }
    //     if (nameeparent && *path == '\0') {
    //         // Stop one level early.
    //         iunlock(fat_ep);
    //         return fat_ep;
    //     }
    //     if ((next = dirlookup(fat_ep, name, 0)) == 0) {
    //         iunlockput(fat_ep);
    //         return 0;
    //     }
    //     iunlockput(fat_ep);
    //     fat_ep = next;
    // }
    // if (nameeparent) {
    //     eput(fat_ep);
    //     return 0;
    // }
    return fat_ep;
}

fat_entry_t* fat32_name_fat_entry(char *path) {
    char name[PATH_LONG_MAX];// 260
    return namex(path, 0, name);
}

fat_entry_t* fat32_name_fat_entry_parent(char *path, char *name) {
    return namex(path, 1, name);
}

fat_entry_t* fat32_fat_entry_dup(fat_entry_t *fat_ep) {
    acquire(&fat_entry_table.lock);
    fat_ep->ref++;
    release(&fat_entry_table.lock);
    return fat_ep;
}

void fat32_fat_entry_update(fat_entry_t *fat_ep) {
    struct buffer_head *bp;
    // struct dinode *dip;
    
    // bp = bread(fat_ep->dev, IBLOCK(fat_ep->inum, sb));
    // dip = (struct dinode *)bp->data + fat_ep->inum % IPB;
    // dip->type = fat_ep->type;
    // dip->major = fat_ep->major;
    // dip->minor = fat_ep->minor;
    // dip->nlink = fat_ep->nlink;
    // dip->size = fat_ep->size;
    // memmove(dip->addrs, fat_ep->addrs, sizeof(fat_ep->addrs));
    // log_write(bp);
    // brelse(bp);
}

void fat32_fat_entry_trunc(fat_entry_t* fat_ep) {
    struct buffer_head *bp;

    FAT_term_t iter_n = fat_ep->cluster_start;
    uint32 FAT_s_n;
    uint32 FAT_s_offset;
    FAT_term_t end=0;

    int cnt = fat32_fat_travel(iter_n, &end);
    Info("%d\n",cnt);
    panic("stop");
    while(!ISEOF(iter_n))
    {
        FAT_s_n =  ThisFATSecNum(iter_n);
        FAT_s_offset = ThisFATEntOffset(iter_n);
        // bp = bread(fat_ep->fatfs_obj->dev, FAT_s_n);
        bp = bread(ROOTDEV,FAT_s_n);
        Info_R("%d %d\n",FAT_s_n,fat_ep->fatfs_obj->dev);
        // Show_bytes((byte_pointer)&bp->data, sizeof(bp->data));
        FAT_term_t fat_next=FAT32ClusEntryVal(bp->data,iter_n);
        SetFAT32ClusEntryVal(bp->data,iter_n,EOC_MASK);
        // Info("%d\n",fat_next);
        bwrite(bp);
        brelse(bp);
        iter_n=fat_next;
    }
    fat_ep->cluster_start = -1;
    fat_ep->cluster_end = -1;
    fat_ep->parent_off = -1;
    fat_ep->cluster_cnt = -1;
    fat_ep->DIR_FileSize = 0;
    fat32_fat_entry_update(fat_ep);
}

// 获取fat_entry的锁
void fat32_fat_entry_lock(fat_entry_t* fat_ep) {
    if (fat_ep == 0 || fat_ep->ref < 1)
        panic("ilock");
    acquiresleep(&fat_ep->lock);
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


