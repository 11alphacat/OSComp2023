#include "kernel_test.h"
#include "fs/fat/fat32_disk.h"
#include "fs/fat/fat32_mem.h"
extern struct _superblock fat32_sb;

void fat32_test_functions(void) {
    // fat_entry_t* fat_root = fat32_name_fat_entry(global_fatfs.root_entry.fname);
    struct _inode *inode_dir = fat32_name_inode("/hello");
    return;
}