#include "common.h"
#include "fs/fat/fat32.h"
#include "memory/list_alloc.h"
#include "fs/bio.h"
#include "atomic/spinlock.h"
#include "kernel/trap.h"

struct FATFS fatfs_root;

void panic(char *) __attribute__((noreturn));

int fat32_boot_sector_parser(FATFS_t *fatfs, fat_bpb_t *bpb) {
    // memmove((void *)fatfs->volbase, bpb->RsvdSecCnt, sizeof(bpb->RsvdSecCnt));
    // fatfs->volbase = bpb->RsvdSecCnt;

    return 0;
}

// Bootblock
FRESULT fat32_mount(int dev, FATFS_t *fatfs) {
    fatfs = (FATFS_t *)kalloc();
    if (fatfs == 0)
        panic("allocation fail, no space");
    struct buffer_head *bp;
    bp = bread(dev, 0);

    /*
    uchar   n_fats;// Number of FATs (1 or 2)
    uint    n_sectors_fat; // Number of sectors per FAT
       
    uint    volbase;  // Volume base sector
    uint	dirbase;  // Root directory base sector
    uint    fatbase;  // FAT base sector
    uint    database; // Data base sector

    uint    sector_size;// size of a sector
    uint    cluster_size;// size of a cluster
    uint    sector_per_cluster;// sector of a cluster

    struct FAT32_Fsinfo fsinfo;
    access window*/
    // uint   winsect;
    // uchar win[FF_MAX_SS];
    fatfs->dev = dev;
    initlock(&fatfs->lock, "fatfs_lock");

    fat32_boot_sector_parser(fatfs, (fat_bpb_t *)bp->data);

    return FR_OK;
}

FRESULT fat32_mkfs(void) {
    return FR_OK;
}

FRESULT fat32_open(void) {
    return FR_OK;
}

FRESULT fat32_close(void) {
    return FR_OK;
}