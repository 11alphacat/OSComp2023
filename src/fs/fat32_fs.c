#include "common.h"
#include "fs/fat/fat32_fs.h"
#include "fs/fat/fat32_entry.h"
#include "atomic/sleeplock.h"
#include "memory/list_alloc.h"
#include "fs/bio.h"
#include "debug.h"

FATFS_t global_fatfs;

int fat32_fs_mount(int dev, FATFS_t *fatfs) {
    /*先分配一个物理块，用kalloc，类型为FATFS_t*/
    // fatfs = (FATFS_t *)kalloc();

    /*初始化fatfs*/
    fatfs->dev = dev;
    initlock(&fatfs->lock, "fatfs");

    /*用bread，读取设备号为dev，第0号扇区*/
    struct buffer_head *bp;
    bp = bread(dev, 0);
    fat32_boot_sector_parser(fatfs, (fat_bpb_t *)bp->data);

    /*用bread，读取设备号为dev，第1号扇区*/
    bp = bread(dev, 1);
    fat32_fsinfo_parser(fatfs, (fsinfo_t *)bp->data);
    /*用brelse 释放缓存*/
    // brelse(bp);
    /* 调用fat32_root_entry_init，获取到root根目录的fat_entry*/
    // panic("hello\n");
    fatfs->root_entry = fat32_root_entry_init(fatfs);
    // TODO： fat_dirty
    // fat的读脏写回链表
    // panic("fs_mount");

    panic("fs_mount");
    return FR_OK;
}

int fat32_fsinfo_parser(FATFS_t *fatfs, fsinfo_t *fsinfo) {
    Info_R("=============FSINFO==========\n");
    /*初始化fatfs->fsinfo的一些值*/
    Info("LeadSig : ");
    Show_bytes((byte_pointer)&fsinfo->LeadSig, sizeof(fsinfo->LeadSig));

    Info("StrucSig : ");
    Show_bytes((byte_pointer)&fsinfo->StrucSig, sizeof(fsinfo->StrucSig));

    Info_R("Free_Count : %d\n", fsinfo->Free_Count);

    Info_R("Nxt_Free : %d\n", fsinfo->Nxt_Free);

    Info("TrailSig : ");

    Show_bytes((byte_pointer)&fsinfo->TrailSig, sizeof(fsinfo->TrailSig));
    fatfs->free_count = fsinfo->Free_Count;
    fatfs->nxt_free = fsinfo->Nxt_Free;
    /*打印信息*/
    return FR_OK;
}

int fat32_boot_sector_parser(FATFS_t *fatfs, fat_bpb_t *fat_bpb) {
    Info_R("=============BOOT Sector==========\n");
    /*初始化fatfs的各个参数*/
    fatfs->sector_size = fat_bpb->BytsPerSec;
    fatfs->sector_per_cluster = fat_bpb->SecPerClus;
    fatfs->n_fats = fat_bpb->NumFATs;
    fatfs->n_sectors = fat_bpb->TotSec32;
    fatfs->n_sectors_fat = fat_bpb->FATSz32;
    fatfs->cluster_size = fatfs->sector_size * fatfs->sector_per_cluster;
    fatfs->rootbase = fat_bpb->RootClus;
    fatfs->fatbase = fat_bpb->RsvdSecCnt;
    /*然后是打印fat32的所有信息*/
    Info_R("Jmpboot : ");
    Show_bytes((byte_pointer)&fat_bpb->Jmpboot, sizeof(fat_bpb->Jmpboot));
    Info_R("OEMNAME : %s\n", fat_bpb->OEMName);

    Info_R("BytsPerSec : %d\n", fat_bpb->BytsPerSec);
    Info_R("SecPerClus : %d\n", fat_bpb->SecPerClus);

    Info_R("RsvdSecCnt : %d\n", fat_bpb->RsvdSecCnt);

    Info_R("NumFATs : %d\n", fat_bpb->NumFATs);

    Info("RootEntCnt : ");
    Show_bytes((byte_pointer)&fat_bpb->RootEntCnt, sizeof(fat_bpb->RootEntCnt));

    Info("TotSec16 : %d\n", fat_bpb->TotSec16);
    Info("Media : ");
    Show_bytes((byte_pointer)&fat_bpb->Media, sizeof(fat_bpb->Media));

    Info("FATSz16 : ");
    Show_bytes((byte_pointer)&fat_bpb->FATSz16, sizeof(fat_bpb->FATSz16));

    Info_R("SecPerTrk : %d\n", fat_bpb->SecPerTrk);
    Info_R("NumHeads : %d\n", fat_bpb->NumHeads);
    Info("HiddSec : %d\n", fat_bpb->HiddSec);

    Info_R("TotSec32 : %d\n", fat_bpb->TotSec32);

    Info_R("FATSz32 : %d\n", fat_bpb->FATSz32);

    Info("ExtFlags : ");
    printf_bin(fat_bpb->ExtFlags, sizeof(fat_bpb->ExtFlags));

    Info("FSVer : ");
    Show_bytes((byte_pointer)&fat_bpb->FSVer, sizeof(fat_bpb->FSVer));

    Info("RootClus : %d\n", fat_bpb->RootClus);

    Info_R("FSInfo : %d\n", fat_bpb->FSInfo);

    Info_R("BkBootSec : %d\n", fat_bpb->BkBootSec);

    Info("DrvNum : %d\n", fat_bpb->DrvNum);
    Info("BootSig : ");
    Show_bytes((byte_pointer)&fat_bpb->BootSig, sizeof(fat_bpb->BootSig));

    Info("VolID : ");
    Show_bytes((byte_pointer)&fat_bpb->VolID, sizeof(fat_bpb->VolID));

    Info("VolLab : ");
    Show_bytes((byte_pointer)&fat_bpb->VolLab, sizeof(fat_bpb->VolLab));

    Info("FilSysType : %s\n", fat_bpb->FilSysType);

    Info("BootCode : ");
    Show_bytes((byte_pointer)&fat_bpb->BootCode, sizeof(fat_bpb->BootCode));

    Info("BootSign : ");
    Show_bytes((byte_pointer)&fat_bpb->BootSign, sizeof(fat_bpb->BootSign));

    // panic("boot sector parser");
    return FR_OK;
}

uchar ChkSum(uchar* pFcbName){
    uchar Sum = 0;
    for (short FcbNameLen=11; FcbNameLen!=0; FcbNameLen--) 
    {
        Sum = ((Sum & 1) ? 0x80 : 0) + (Sum >> 1) + *pFcbName++;
    }
    return (Sum);
}