#include "common.h"
#include "fs/fat/fat32_fs.h"
#include "fs/fat/fat32.h"
#include "atomic/sleeplock.h"
#include "memory/list_alloc.h"
#include "fs/bio.h"
#include "debug.h"


// 根目录占用的扇区数
#define RootDirSectors(BPB_RootEntCnt, BPB_BytsPerSec) ((BPB_RootEntCnt * 32) + (BPB_BytsPerSec - 1)) / BPB_BytsPerSec // (0*32+512-1)/512 FAT32 没有Root Directory 的概念，所以一直是0

// 第一个数据区域的扇区号
#define FirstDataSector(BPB_ResvdSecCnt, BPB_NumFATs, FATSz, RootDirSectors) ((BPB_ResvdSecCnt) + ((BPB_NumFATs) * (FATSz)) + (RootDirSectors))
// 假设一个FAT32区域占2017个扇区：32+(2*2017)+0

#define FirstSectorofCluster(N, BPB_SecPerClus, FirstDataSector) (((N)-2) * (BPB_SecPerClus)) + (FirstDataSector)
// 假设一个簇两个扇区：(N-2)*2+32+(2*2017)+0

#define DataSec(TotSec, BPB_ResvdSecCnt, BPB_NumFATs, FATSz, RootDirSectors) ((TotSec) - ((BPB_ResvdSecCnt) + ((BPB_NumFATs) * (FATSz)) + (RootDirSectors)))
// 就是用总的数量减去保留扇区，然后减去FAT的扇区

// 数据区域的总簇数
#define CountofClusters(DataSec, BPB_SecPerClus) ((DataSec) / (BPB_SecPerClus))

#define FATOffset(N)  (N) * 4
#define ThisFATSecNum(N, BPB_BytsPerSec, BPB_ResvdSecCnt) ((BPB_ResvdSecCnt)+(FATOffset(N))/(BPB_BytsPerSec))
// 保留区域的扇区个数+一个扇区对应4字节的FAT表项的偏移/一个扇区多少个字节，就可以算出这个FAT表项在那个扇区
#define ThisFATEntOffset(FATOffset,BPB_BytesPerSec) ((FATOffset) % (BPB_BytsPerSec))
// 算出FAT表项在对应扇区的具体多少偏移的位置

// 读取表项内容和写入表项内容
#define FAT32ClusEntryVal(SecBuff,ThisFATEntOffset) ((*((DWORD *) &(SecBuff)[(ThisFATEntOffset)])) & 0x0FFFFFFF)
// #define SetFAT32ClusEntryVal()
// {
//     FAT32ClusEntryVal = FAT32ClusEntryVal & 0x0FFFFFFF;
//     *((DWORD *) &SecBuff[ThisFATEntOffset]) =(*((DWORD *) &SecBuff[ThisFATEntOffset])) & 0xF0000000;
//     *((DWORD *) &SecBuff[ThisFATEntOffset]) =(*((DWORD *) &SecBuff[ThisFATEntOffset])) | FAT32ClusEntryVal;
// }

#define ISEOF(FATContent) ((FATContent) >= 0x0FFFFFF8)

#define ClnShutBitMask 0x08000000
// If bit is 1, volume is "clean".If bit is 0, volume is "dirty". 
#define HrdErrBitMask 0x04000000
// f this bit is 1, no disk read/write errors were encountered.If this bit is 0, the file system driver encountered a disk I/O error on theVolume the last time it was mounted, 
// which is an indicator that some sectorsmay have gone bad on the volume.
#define EOC 0xFFFFFFFF
#define FREE 0x00000000

FATFS_t global_fatfs;
// maximum file entry in memory
#define NENTRY 10
struct {
    spinlock_t *lock;
    fat_entry_t fat_table[NENTRY];
} fat_entry_table;

// void FAT_type(int CountofClusters) {
//     If(CountofClusters < 4085) {
//         panic("FAT12");
//     }
//     else if (CountofClusters < 65525) { /* Volume is FAT16 */
//         panic("FAT16");
//     }
//     else { /* Volume is FAT32 */
//         panic("FAT32");
//     }
// }

FRESULT fat32_fs_init() {
    fat_entry_t *entry;
    for (entry = fat_entry_table.fat_table; entry < &fat_entry_table.fat_table[NENTRY]; entry++) {
        memset(entry, 0, sizeof(fat_entry_t));
        initsleeplock(&entry->lock, "fat_entry");
    }
    return FR_OK;
}

FRESULT fat32_fs_mount(int dev, FATFS_t *fatfs) {
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
    int root_sec=FirstSectorofCluster(2, global_fatfs.sector_per_cluster, FirstDataSector(global_fatfs.fatbase, global_fatfs.n_fats, global_fatfs.n_sectors_fat, 0));
    // 需要返回一个OK的FRESULT
    Info("%d\n",root_sec);
    bp = bread(dev, root_sec);

    dirent_l_t* tmp = (dirent_l_t*)(bp->data);
    
    Show_bytes((byte_pointer)&tmp, sizeof(tmp));

    return FR_OK;
}

FRESULT fat32_fsinfo_parser(FATFS_t *fatfs, fsinfo_t *fsinfo) {
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

FRESULT fat32_boot_sector_parser(FATFS_t *fatfs, fat_bpb_t *fat_bpb) {
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
