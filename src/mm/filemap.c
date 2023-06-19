#include "lib/radix-tree.h"
#include "memory/filemap.h"
#include "atomic/spinlock.h"
#include "memory/buddy.h"
#include "memory/allocator.h"
#include "fs/mpage.h"
#include "atomic/ops.h"
#include "debug.h"
#include "kernel/trap.h"

// add
int add_to_page_cache_atomic(struct page *page, struct address_space *mapping, uint64 index) {
    page->mapping = mapping;
    page->index = index;

    acquire(&mapping->tree_lock);
    int error = radix_tree_insert(&mapping->page_tree, index, page);
    if (likely(!error)) {
        mapping->nrpages++;
    } else {
        panic("add_to_page_cache : error\n");
    }
    release(&mapping->tree_lock);
    return error;
}

// find
struct page *find_get_page_atomic(struct address_space *mapping, uint64 index, int lock) {
    struct page *page;

    acquire(&mapping->tree_lock);
    page = (struct page *)radix_tree_lookup_node(&mapping->page_tree, index);
    release(&mapping->tree_lock);

    if (page) {
        page_cache_get(page);
        if (lock)
            acquire(&page->lock);
    }

    return page;
}

// read ahead
uint64 max_sane_readahead(uint64 nr) {
    return MIN(PGROUNDUP(nr), DIV_ROUND_UP(FREE_RATE(READ_AHEAD_RATE), PGSIZE));
}

// read using mapping
ssize_t do_generic_file_read(struct address_space *mapping, int user_dst, uint64 dst, uint off, uint n) {
    struct inode *ip = mapping->host;
    ASSERT(ip->i_size > 0);
    ASSERT(n > 0);

    uint64 index = off >> PGSHIFT; // page number
    uint64 offset = PGMASK(off);   // offset in a page
    uint64 end_index = (ip->i_size - 1) >> PGSHIFT;
    uint32 isize = ip->i_size;
    uint64 read_sane_cnt = 0; // 1, 2, 3, ...

    uint64 pa;
    uint64 nr, len;

    ssize_t retval = 0;
    while (1) {
        struct page *page;

        /* nr is the maximum number of bytes to copy from this page */
        nr = PGSIZE;
        if (index >= end_index) {
            if (index > end_index)
                goto out;
            // if index == end_index
            nr = (PGMASK((isize - 1))) + 1;
            // it is larger than offset of off
            if (nr <= offset) {
                goto out;
            }
        }
        nr = nr - offset;
        /* Find the page */
        page = find_get_page_atomic(mapping, index, 0); // not acquire the lock of page
        if (page == NULL) {
            read_sane_cnt = max_sane_readahead(n - retval);
            ASSERT(read_sane_cnt > 0);
            pa = mpage_readpages(ip, index, read_sane_cnt, 1); // must read from disk
        } else {
            pa = page_to_pa(page);
        }

        // similar to fat32_inode_read
        // it is illegal to read beyond isize!!! (maybe it is reasonable to fill zero)
        len = MIN(MIN(n - retval, nr), isize - offset);

        if (either_copyout(user_dst, dst, (void *)(pa + offset), len) == -1) {
            panic("do_generic_file_read : copyout error\n");
        }

        // off、retval、src
        // unit is byte
        off += len;
        retval += len;
        dst += len;

        // index and offset
        // page_no + page_offset
        index = (off >> PGSHIFT);
        offset = PGMASK(off);
    }

out:
    return retval;
}

// write using mapping
ssize_t do_generic_file_write(struct address_space *mapping, int user_src, uint64 src, uint off, uint n) {
    struct inode *ip = mapping->host;
    ASSERT(n > 0);
    uint64 index = off >> PGSHIFT; // page number
    uint64 offset = PGMASK(off);   // offset in a page
    uint64 pa;
    uint64 nr, len;

    uint64 isize = PGROUNDUP(ip->i_size);

    ssize_t retval = 0;
    while (1) {
        struct page *page;

        /* nr is the maximum number of bytes to copy from this page */
        nr = PGSIZE - offset;
        page = find_get_page_atomic(mapping, index, 0); // not acquire lock

        if (page == NULL) {
            int read_from_disk = -1;
            if (!OUTFILE(index, isize) && NOT_FULL_PAGE(offset, n - retval)) {
                // need read page in disk
                read_from_disk = 1;
            } else {
                read_from_disk = 0;
            }
            pa = mpage_readpages(ip, index, 1, read_from_disk); // just read one page
            page = pa_to_page(pa);
        } else {
            pa = page_to_pa(page);
        }

        // similar to fat32_inode_read
        len = MIN(n - retval, nr);
        if (either_copyin((void *)(pa + offset), user_src, src, len) == -1) {
            panic("do_generic_file_write : copyin error\n");
        }

        // set page dirty
        set_page_flags(page, PG_dirty);
        radix_tree_tag_set(&mapping->page_tree, index, PAGECACHE_TAG_DIRTY);

        // put and release
        page_cache_put(page);
        release(&page->lock);

        // off、retval、src
        // unit is byte
        off += len;
        retval += len;
        src += len;

        // index and offset
        // page_no + page_offset
        index = off >> PGSHIFT;
        offset = PGMASK(off);
    }

    return retval;
}