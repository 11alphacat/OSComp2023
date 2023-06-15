#include "lib/radix-tree.h"
#include "memory/filemap.h"
#include "atomic/spinlock.h"
#include "memory/buddy.h"
#include "atomic/ops.h"
#include "debug.h"

// add
int add_to_page_cache_atomic(struct page *page, struct address_space *mapping,
                             uint32 offset, gfp_t gfp_mask) {
    page->mapping = mapping;
    page->index = offset;

    acquire(&mapping->tree_lock);
    int error = radix_tree_insert(&mapping->page_tree, offset, page);
    if (likely(!error)) {
        mapping->nrpages++;
    } else {
        panic("add_to_page_cache : error\n");
    }
    release(&mapping->tree_lock);
    return error;
}

// find
struct page *find_get_page_atomic(struct address_space *mapping, uint32 offset, int lock) {
//     struct page *page;
//     acquire(&mapping->tree_lock);
//     page = (struct page *)radix_tree_lookup_node(&mapping->page_tree, offset);
//     release(&mapping->tree_lock);

//     if(page)
//         page_cache_get(page);
//     if(lock)
//         acquire(&page->lock);

//     return page;
// }

// // read using mapping
// ssize_t read_using_mapping(struct address_space *mapping, int user_src, uint64 src, uint off, uint n) {
//     struct inode *inode = mapping->host;
//     ASSERT(inode->i_size>0);
//     ASSERT(n>0);

//     uint64 index = off >> PGSHIFT;
// 	uint64 offset = PGMASK(off);
// 	uint64 end_index = (inode->i_size-1) >> PGSHIFT;
//     uint32 isize = inode->i_size;
//     uint len = n;

//     ssize_t retval = 0;
//     while(1) {
//         struct page *page;
// 		uint64 nr, ret;
// 		int ra;
        
//         /* nr is the maximum number of bytes to copy from this page */
//         nr = PGSIZE;
// 		if (index >= end_index) {
// 			if (index > end_index)
// 				goto out;
//             // if index == end_index
// 			nr = (PGMASK((isize-1))) + 1;
//             // it is larger than offset of off
// 			if (nr <= offset) {
// 				goto out;
// 			}
// 		}
//         nr = nr - offset;
//         /* Find the page */
//         page = find_get_page_atomic(mapping, index, 0);// not acquire the lock of page
//         if (unlikely(page == NULL)) {
// 			/*
// 			 * We have a HOLE, zero out the user-buffer for the length of the hole or request.
// 			 */
// 			// ret = len < nr ? len : nr;
// 			// // if (clear_user(buf, ret))
//             // if()
// 			// 	ra = -1;
// 			// else
// 			// 	ra = 0;
// 		} else {
// 			/*
// 			 * We have the page, copy it to user space buffer.
// 			 */

// 			// ra = hugetlbfs_read_actor(page, offset, buf, len, nr);
			
            
//             ret = ra;
// 		}

//         offset += ret;
// 		retval += ret;
// 		len -= ret;
// 		index += (offset >> PGSIZE);
// 		offset = PGMASK(offset);
//     }

// out:
//     return retval;
    return NULL;
}


// write using mapping
ssize_t write_using_mapping(struct address_space *mapping, int user_src, uint64 src, uint off, uint n) {
    // struct inode *inode = mapping->host;
    return 0;
}




