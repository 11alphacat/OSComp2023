#ifndef __FILEMAP_H__
#define __FILEMAP_H__
#include "fs/vfs/fs.h"
#include "memory/buddy.h"

#define READ_AHEAD_RATE 20

#define OUTFILE(index, pgsize) ((index) > (pgsize))
#define NOT_FULL_PAGE(offset, restval) ((offset) != 0 || (restval) < PGSIZE)

int add_to_page_cache_atomic(struct page *page, struct address_space *mapping, uint64 index);
struct page *find_get_page_atomic(struct address_space *mapping, uint64 index, int lock);
uint64 max_sane_readahead(uint64 nr);
ssize_t do_generic_file_read(struct address_space *mapping, int user_src, uint64 src, uint off, uint n);
ssize_t do_generic_file_write(struct address_space *mapping, int user_src, uint64 src, uint off, uint n);

#endif