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
void block_full_pages(struct inode *ip, struct bio *bio_p, uint64 src, uint64 index, uint64 cnt, int alloc) {
    // fill the bio using fat32_get_block
    uint32 off = index * PGSIZE;
    uint32 n = cnt * PGSIZE;
    uint32 bsize = ip->i_sb->sector_size;

    // pay attention to alloc!!!
    int blocks_n = fat32_get_block(ip, bio_p, off, n, alloc);
    if (alloc == 1) // it is ok, if only read
        ASSERT(blocks_n * bsize == n);

    // copy bio_vec into dst
    struct bio_vec *vec_cur = NULL;
    list_for_each_entry(vec_cur, &bio_p->list_entry, list) {
        vec_cur->data = (uchar *)src;
        src += vec_cur->block_len * bsize;
    }
}

// read/write more than one page
void fat32_rw_pages(struct inode *ip, uint64 src, uint64 index, int rw, uint64 cnt, int alloc) {
    struct bio bio_cur;

    // init bio
    INIT_LIST_HEAD(&bio_cur.list_entry);
    bio_cur.bi_rw = rw;
    bio_cur.bi_bdev = ip->i_dev;

    // fill bio
    block_full_pages(ip, &bio_cur, src, index, cnt, alloc);

    // submit bio
    if (!list_empty(&bio_cur.list_entry)) {
        submit_bio(&bio_cur, 1); // free bio_vec of bio
    }
}

// read/write more than one page using page batch
void fat32_rw_pages_batch(struct inode *ip, struct Page_entry *p_entry, int rw, int alloc) {
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
                // struct page *page_cur = pa_to_page(p_cur_in->pa);
                // page_cache_put(page_cur);
                // release(&page_cur->lock);
            } else {
                break;
            }
        }
#ifdef __DEBUG_PAGE_CACHE__
        // if(batch_size > 1) {
        //     if(rw == DISK_READ)
        //         printfCYAN("read : batchsize : %d, index from %d to %d\n", batch_size, p_tmp_head_in->index, p_tmp_head_in->index + batch_size - 1);
        //     else
        //         printfBlue("write : batchsize : %d, index from %d to %d\n", batch_size, p_tmp_head_in->index, p_tmp_head_in->index + batch_size - 1);
        // }
#endif
        // release(&pa_to_page(p_tmp_head_in->pa)->lock); // !!! maybe the lock protecting page is not needed ??
        fat32_rw_pages(ip, p_tmp_head_in->pa, p_tmp_head_in->index, rw, batch_size, alloc); // don't write batch_size as batch_size * PGSIZE
    }
}

// read pages
// index : page start index
// cnt : page count
// read_from_disk : need read from disk ??
// return : pa of the first page
uint64 mpage_readpages(struct inode *ip, uint64 index, uint64 cnt, int read_from_disk, int alloc) {
    struct Page_entry p_entry;
    struct address_space *mapping = ip->i_mapping;

    ASSERT(cnt > 0);

    INIT_LIST_HEAD(&p_entry.entry); // !!!
    p_entry.n_pages = 0;            // !!!!!! bug

    uint64 first_pa = 0;
    // we want cnt * pagesize is continous
    if ((first_pa = (uint64)kzalloc(PGSIZE * cnt)) == 0) {
        panic("mpage_readpages : no enough memory\n");
    }

    uint64 pa = 0;
    for (int i = 0; i < cnt; i++) {
        if (find_get_page_atomic(mapping, index, 0)) {
            // find it, not holding lock
            continue;
        }

        // pa and page
        pa = first_pa + i * PGSIZE;
        struct page *page = pa_to_page(pa);

        // refcnt ++ and acquire lock
        // page_cache_get(page);
        // acquire(&page->lock);

        add_to_page_cache_atomic(page, mapping, index);

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
        list_add_tail(&p_item->list, &p_entry.entry);
        // bug like this : list_add_tail(&p_entry.entry, &p_item->list)
        p_entry.n_pages++;
    }

    if (read_from_disk)
        // read pages using page list
        fat32_rw_pages_batch(ip, &p_entry, DISK_READ, alloc);

    return first_pa;
}

// write pages
void mpage_writepage(struct inode *ip, int alloc) {
    struct address_space *mapping = ip->i_mapping;
    if (mapping->page_tree.height == 0 && mapping->page_tree.rnode == NULL) {
        return;
    }

    struct Page_entry p_entry;
    INIT_LIST_HEAD(&p_entry.entry); // !!!
    p_entry.n_pages = 0;// !!! bug

    int ret = radix_tree_general_gang_lookup_elements(&(mapping->page_tree), &p_entry, page_list_add,
                                                      0, maxitems_invald, PAGECACHE_TAG_DIRTY);
    ASSERT(ret > 0);
    if(mapping->page_tree.height == 0 && ret != 1) {
        panic("mpage_writepage : error\n");
    }

    // write pages using page list
    fat32_rw_pages_batch(ip, &p_entry, DISK_WRITE, alloc);
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
