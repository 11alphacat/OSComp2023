#include "test.h"
#include "fs/fat/fat32_disk.h"
#include "fs/fat/fat32_mem.h"
#include "fs/vfs/fs.h"
#include "debug.h"
extern struct _superblock fat32_sb;

void fat32_test_functions(void) {
    // fat_entry_t* fat_root = fat32_name_fat_entry(global_fatfs.root_entry.fname);
    // struct _inode *inode_dir = fat32_name_inode("/hello");

    // struct _inode *inode_new = fat32_inode_create("/hello/raw/applepeach",  ATTR_DIRECTORY);
    // struct _inode *inode_new = fat32_inode_create("/apple/raw",  ATTR_DIRECTORY);
    // struct _inode *inode_new = fat32_inode_create("/hello/b.txt",  T_FILE);
    // struct _inode *inode_new = fat32_inode_create("/hello/userkernelapples.txt",  T_FILE);
    // struct _inode *inode_new = fat32_inode_create("/test_dir/userkernelap.txt", T_FILE);

    // struct _inode *inode_new = fat32_inode_create("/test_dir/apple.txt", T_FILE);
    // uint sector_num = FATINUM_TO_SECTOR(inode_new->i_ino);
    // uint sec_pos = DEBUG_SECTOR(inode_new, sector_num);//debug
    // char tmp[30];
    // fat32_time_parser(inode_new, &inode_new->fat32_i.DIR_CrtTime, tmp, 1);
    // printf("%s\n", tmp);

    // fat32_date_parser(&inode_new->fat32_i.DIR_CrtDate, tmp);
    // printf("%s\n", tmp);

    // fat32_time_parser(inode_new, &inode_new->fat32_i.DIR_WrtTime, tmp, 1);
    // printf("%s\n", tmp);

    // fat32_date_parser(&inode_new->fat32_i.DIR_LstAccDate, tmp);
    // printf("%s\n", tmp);

    // fat32_date_parser(&inode_new->fat32_i.DIR_WrtDate, tmp);
    // printf("%s\n", tmp);

    // fat32_inode_delete(inode_new->parent, inode_new);
    // int ms;
    // FAT_time_t time_now = fat32_inode_get_time(&ms);

    // char tmp[30];
    // fat32_time_parser(&time_now, tmp, &ms);
    // printf("%s\n", tmp);

    return;
}
