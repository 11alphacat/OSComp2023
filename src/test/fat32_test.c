#include "kernel_test.h"
#include "fs/fat/fat32_disk.h"
#include "fs/fat/fat32_mem.h"
extern FATFS_t global_fatfs;

void fat32_test_functions(void)
{
    
    // fat_entry_t* fat_root = fat32_name_fat_entry(global_fatfs.root_entry.fname);
    fat_entry_t* fat_dir = fat32_name_fat_entry("/hello");
    return;
}