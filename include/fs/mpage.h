#ifndef __MPAGE_H__
#define __MPAGE_H__
#include "common.h"
#include "lib/list.h"
#include "lib/radix-tree.h"
#include "memory/buddy.h"

// we use page list to replace page array
struct Page_entry {
    struct list_head entry;
    uint64 n_pages;
};

// we use pa(physical address) to replace page pointer
struct Page_item {
    uint64 pa;
    uint64 index;
    struct list_head list;
};

#define PAGE_ADJACENT(p_nxt, p_cur) (((p_nxt->pa) == (p_cur->pa) + PGSIZE) && (p_nxt->index == p_cur->index + 1))

void block_full_pages(struct inode *ip, struct bio *bio_p, uint64 dst, uint64 index, uint64 cnt);
void fat32_rw_pages(struct inode *ip, uint64 dst, uint64 index, int rw, uint64 cnt);
void fat32_rw_pages_batch(struct inode *ip, struct Page_entry *p_entry, int rw);
uint64 mpage_readpages(struct inode *ip, uint64 index, uint64 cnt, int read_from_disk);
void mpage_writepage(struct inode *ip);
void page_list_add(void *entry, void *item, uint64 index, void *node);
void page_list_delete(void *entry, void *item, uint64 index, void *node);

#endif