#include "common.h"
#include "fs/fat/fat32_disk.h"
#include "fs/fat/fat32_mem.h"
#include "fs/vfs/fs.h"
#include "atomic/sleeplock.h"
#include "memory/list_alloc.h"
#include "fs/bio.h"
#include "debug.h"
#include "test.h"
#include "param.h"
#include "fs/fat/fat32_stack.h"
#include "test.h"

extern struct _superblock fat32_sb;

int fat32_fs_mount(int dev, struct _superblock *sb) {
    /* superblock initialization */
    sb->s_op = TODO();
    sb->s_dev = dev;
    initlock(&sb->lock, "fat32_sb");

    /* read boot sector in sector 0 */
    struct buffer_head *bp;
    bp = bread(dev, SECTOR_BPB);
    fat32_boot_sector_parser(sb, (fat_bpb_t *)bp->data);
    brelse(bp);

    /* read fsinfo sector in sector 1 */
    bp = bread(dev, SECTOR_FSINFO);
    fat32_fsinfo_parser(sb, (fsinfo_t *)bp->data);
    brelse(bp);

    /* 调用fat32_root_entry_init，获取到root根目录的fat_entry*/
    sb->fat32_sb_info.root_entry = fat32_root_inode_init(sb);
    sb->s_mount = sb->fat32_sb_info.root_entry;

    // snprintf_test();
    // printf_test();
    fat32_test_functions();
    panic("fs_mount");
    return FR_OK;
}

int fat32_fsinfo_parser(struct _superblock *sb, fsinfo_t *fsinfo) {
    /* superblock fsinfo fileds initialization */
    sb->fat32_sb_info.free_count = fsinfo->Free_Count;
    sb->fat32_sb_info.nxt_free = fsinfo->Nxt_Free;

    ////////////////////////////////////////////////////////////////////////////////
    Info_R("=============FSINFO==========\n");
    Info("LeadSig : ");
    Show_bytes((byte_pointer)&fsinfo->LeadSig, sizeof(fsinfo->LeadSig));

    Info("StrucSig : ");
    Show_bytes((byte_pointer)&fsinfo->StrucSig, sizeof(fsinfo->StrucSig));

    Info_R("Free_Count : %d\n", fsinfo->Free_Count);

    Info_R("Nxt_Free : %d\n", fsinfo->Nxt_Free);

    Info("TrailSig : ");

    Show_bytes((byte_pointer)&fsinfo->TrailSig, sizeof(fsinfo->TrailSig));
    return FR_OK;
}

int fat32_boot_sector_parser(struct _superblock *sb, fat_bpb_t *fat_bpb) {
    Info_R("=============BOOT Sector==========\n");
    /* superblock initialization */
    // common
    sb->sector_size = fat_bpb->BytsPerSec;
    sb->n_sectors = fat_bpb->TotSec32;
    sb->sectors_per_block = fat_bpb->SecPerClus;

    // specail for fat32 superblock
    sb->fat32_sb_info.fatbase = fat_bpb->RsvdSecCnt;
    sb->fat32_sb_info.n_fats = fat_bpb->NumFATs;
    sb->fat32_sb_info.n_sectors_fat = fat_bpb->FATSz32;
    sb->fat32_sb_info.root_cluster_s = fat_bpb->RootClus;

    // repeat with sb->s_blocksize
    sb->cluster_size = (sb->sector_size) * (sb->sectors_per_block);
    sb->s_blocksize = sb->cluster_size;

    //////////////////////////////////////////////////////////////////
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

    Info("FilSysType : ");
    printf("%.5s\n", fat_bpb->FilSysType);

    Info("BootCode : ");
    Show_bytes((byte_pointer)&fat_bpb->BootCode, sizeof(fat_bpb->BootCode));

    Info("BootSign : ");
    Show_bytes((byte_pointer)&fat_bpb->BootSign, sizeof(fat_bpb->BootSign));

    // panic("boot sector parser");
    return FR_OK;
}
