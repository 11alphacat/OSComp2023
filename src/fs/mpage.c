#include "memory/filemap.h"
#include "memory/allocator.h"
#include "memory/buddy.h"
#include "lib/riscv.h"
#include "lib/radix-tree.h"
#include "fs/vfs/fs.h"
#include "fs/bio.h"
#include "fs/mpage.h"
#include "common.h"
#include "fs/mpage.h"
#include "debug.h"

// index : page index
// cnt : page count
void block_full_pages(struct inode *ip, struct bio *bio_p, uint64 dst, uint64 index, uint64 cnt) {
    // fill the bio using fat32_get_block
    uint32 off = index * PGSIZE;
    uint32 n = cnt * PGSIZE;
    uint32 bsize = ip->i_sb->sector_size;
    int blocks_n = fat32_get_block(ip, bio_p, off, n);
    ASSERT(blocks_n * bsize == n);

    // copy bio_vec into dst
    struct bio_vec *vec_cur = NULL;
    struct bio_vec *vec_tmp = NULL;
    list_for_each_entry_safe(vec_cur, vec_tmp, &bio_p->list_entry, list) {
        memmove((void *)dst, (char *)vec_cur->data, vec_cur->block_len * bsize);
        dst += vec_cur->block_len * bsize;
    }
}

// read/write more than one page
void fat32_rw_pages(struct inode *ip, uint64 dst, uint64 index, int rw, uint64 cnt) {
    struct bio bio_cur;

    INIT_LIST_HEAD(&bio_cur.list_entry);
    bio_cur.bi_rw = rw;
    bio_cur.bi_bdev = ip->i_dev;

    // fill bio
    block_full_pages(ip, &bio_cur, dst, index, cnt);

    // submit bio
    if (list_empty(&bio_cur.list_entry)) {
        submit_bio(&bio_cur);
    }
}

// read/write more than one page using page batch
void fat32_rw_pages_batch(struct inode *ip, struct Page_entry *p_entry, int rw) {
    struct Page_item *p_cur_out = NULL;
    struct Page_item *p_first = NULL;
    int batch_size = 1;

    // out : we don't use list_for_each_entry_safe in order to change p_cur_out in inner
    list_for_each_entry(p_cur_out, &p_entry->entry, list) {
        if (p_first == NULL)
            p_first = p_cur_out;
        struct Page_item *p_cur_in = NULL;
        struct Page_item *p_nxt_in = NULL;
        struct Page_item *p_tmp_head_in = p_cur_out; // !!!
        batch_size = 1;
        // inner :
        list_for_each_entry_safe_condition(p_cur_in, p_nxt_in, &p_tmp_head_in->list, list, p_cur_in != p_first) {
            if (PAGE_ADJACENT(p_nxt_in, p_cur_in)) {
                p_cur_out = p_cur_in; // !!!
                batch_size++;
                struct page *page_cur = pa_to_page(p_cur_in->pa);
                page_cache_put(page_cur);
                release(&page_cur->lock);
            } else {
                break;
            }
        }
        fat32_rw_pages(ip, p_tmp_head_in->pa, p_tmp_head_in->index, rw, batch_size * PGSIZE);
    }
}

// read pages
// index : page start index
// cnt : page count
// read_from_disk : need read from disk ??
// return : pa of the first page
uint64 mpage_readpages(struct inode *ip, uint64 index, uint64 cnt, int read_from_disk) {
    struct Page_entry p_entry;
    struct address_space *mapping = ip->i_mapping;

    ASSERT(cnt > 0);
    uint64 pa_first = 0;
    INIT_LIST_HEAD(&p_entry.entry); // !!!
    for (int i = 0; i < cnt; i++) {
        if (find_get_page_atomic(mapping, index, 0)) {
            // find it, not holding lock
            continue;
        }
        // pa and page
        uint64 pa = 0;
        if ((pa = (uint64)kzalloc(PGSIZE)) == 0) {
            panic("mpage_readpages : no enough memory\n");
        }
        struct page *page = pa_to_page(pa);

        // refcnt ++ and acquire lock
        page_cache_get(page);
        acquire(&page->lock);

        add_to_page_cache_atomic(page, mapping, index);

        if (pa_first == 0) {
            pa_first = pa; // !!!
        }

        if (cnt == 1 && read_from_disk == 0)
            break; // !!!
        // don't forget to release lock !!!

        // page list item
        struct Page_item *p_item = NULL;
        if ((p_item = (struct Page_item *)kzalloc(sizeof(struct Page_item))) == NULL) {
            panic("mpage_readpages : no enough memory\n");
        }
        p_item->index = index++;
        p_item->pa = pa;

        INIT_LIST_HEAD(&p_item->list);

        // join p_item into p_entry
        list_add_tail(&p_entry.entry, &p_item->list);
        p_entry.n_pages++;
    }

    if (read_from_disk)
        // read pages using page list
        fat32_rw_pages_batch(ip, &p_entry, DISK_READ);

    return pa_first;
}

// write pages
void mpage_writepage(struct inode *ip) {
    struct address_space *mapping = ip->i_mapping;
    if (mapping->page_tree.height == 0) {
        return;
    }
    struct Page_entry p_entry;
    INIT_LIST_HEAD(&p_entry.entry); // !!!

    int ret = radix_tree_general_gang_lookup_elements(&(mapping->page_tree), &p_entry, page_list_add,
                                                      0, maxitems_invald, PAGECACHE_TAG_DIRTY);
    ASSERT(ret > 0);

    // write pages using page list
    fat32_rw_pages_batch(ip, &p_entry, DISK_WRITE);
}

// add page item into page list
void page_list_add(void *entry, void *item, uint64 index, void *node) {
    struct Page_entry *p_entry = (struct Page_entry *)entry;
    struct Page_item *p_item = NULL;
    if ((p_item = (struct Page_item *)kzalloc(sizeof(struct Page_item))) == NULL) {
        panic("mpage_readpages : no enough memory\n");
    }

    // p_item
    struct page *page = (struct page *)item;
    p_item->index = index;
    p_item->pa = page_to_pa(page);
    INIT_LIST_HEAD(&p_item->list);

    // join p_item into p_entry
    list_add_tail(&p_entry->entry, &p_item->list);
    p_entry->n_pages++;
}

// delete pagecache
void page_list_delete(void *entry, void *item, uint64 index, void *node) {
    // p_item
    struct page *page = (struct page *)item;

    // put pagecache and free node
    page_cache_put(page);
    radix_tree_node_free((struct radix_tree_node *)node);
}
