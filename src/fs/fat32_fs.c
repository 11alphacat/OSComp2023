#include "common.h"
#include "fs/fat/fat32_fs.h"
#include "atomic/sleeplock.h"

// maximum file entry in memory
#define NENTRY 10
fat_entry_t fat_table[NENTRY];

void fat32_fs_init() {
    fat_entry_t *entry;
    for (entry = fat_table; entry < &fat_table[NENTRY]; entry++) {
        memset(entry, 0, sizeof(fat_entry_t));
        initsleeplock(&entry->lock, "fat_entry");
    }
}
