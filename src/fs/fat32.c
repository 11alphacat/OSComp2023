#include "common.h"
#include "memory/list_alloc.h"
#include "fs/bio.h"
#include "atomic/spinlock.h"
#include "kernel/trap.h"
#include "fs/fat/fat32_fs.h"
#include "debug.h"

#define DIR_FIRST_CLUS(high, low) ((high << 16) | (low))
struct fat_entry *fat32_root_entry_init(FATFS_t *fatfs) {
    /*先用kalloc分配一个物理块*/
    fat_entry_t *root = (fat_entry_t *)kalloc();
    /*初始化root_entry的一些字段*/
    initsleeplock(&root->lock, "fat_entry_root");
    root->parent = root; // 根目录父亲节点是自身
    root->fname[0] = '/';
    root->fname[1] = '\0';
    root->nlink = 1;
    root->ref = 1;
    root->valid = 0;
    root->cluster_start = fatfs->rootbase;
    root->cluster_end = fatfs->rootbase;
    root->parent_off = 0;
    // root->size_in_mem=sizeof(fat_entry_t);
    // Info_R("root size of RAM : %d\n",root->size_in_mem);

    ASSERT(root->cluster_start == 2);
    root->fat_obj = fatfs;

    // dirent_cpy->Attr

    /*初始化dirent_cpy的其他参数*/

    // TODO: 设置i_mapping
    // panic("root entry init\n");
    return root;
}