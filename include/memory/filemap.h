#ifndef __FILEMAP_H__
#define __FILEMAP_H__
#include "fs/vfs/fs.h"
#include "memory/buddy.h"

int add_to_page_cache_atomic(struct page *page, struct address_space *mapping, uint32 offset, gfp_t gfp_mask);
struct page *find_get_page_atomic(struct address_space *mapping, uint32 offset, int lock);

ssize_t read_using_mapping(struct address_space *mapping, int user_src, uint64 src, uint off, uint n);
ssize_t write_using_mapping(struct address_space *mapping, int user_src, uint64 src, uint off, uint n);

#endif