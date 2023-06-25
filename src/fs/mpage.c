#include "common.h"
#include "fs/vfs/fs.h"

// int fat_get_block(struct inode *inode, uint64 iblock, struct buffer_head *bh_result, int create) {
// 	return 0;
// }

// static int fat_writepage(struct page *page, struct writeback_control *wbc)
// {
// 	return block_write_full_page(page, fat_get_block, wbc);
// }

// static int fat_writepages(struct address_space *mapping,
// 			  struct writeback_control *wbc)
// {
// 	return mpage_writepages(mapping, wbc, fat_get_block);
// }

// static int fat_readpage(struct file *file, struct page *page)
// {
// 	return mpage_readpage(page, fat_get_block);
// }

// static int fat_readpages(struct file *file, struct address_space *mapping,
// 			 struct list_head *pages, unsigned nr_pages)
// {
// 	return mpage_readpages(mapping, pages, nr_pages, fat_get_block);
// }
