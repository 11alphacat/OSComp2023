#include "common.h"
#include "fs/fat/fat32_fs.h"
#include "fs/fat/fat32_entry.h"
#include "fs/vfs/fs.h"
#include "atomic/sleeplock.h"
#include "memory/list_alloc.h"
#include "fs/bio.h"
#include "debug.h"
#include "test.h"
#include "param.h"
#include "fs/fat/fat32_stack.h"
#include "kernel_test.h"

// FATFS_t global_fatfs;
struct _superblock fat32_sb;

int fat32_fs_mount(int dev, struct _superblock *sb) {
    /* superblock initialization */
    sb->s_op = TODO();
    sb->s_dev = dev;
    initlock(&sb->lock, "fat32_sb");

    /* read boot sector in sector 0 */
    struct buffer_head *bp;
    bp = bread(dev, 0);
    fat32_boot_sector_parser(sb, (fat_bpb_t *)bp->data);
    brelse(bp);

    /* read fsinfo sector in sector 1 */
    bp = bread(dev, 1);
    fat32_fsinfo_parser(sb, (fsinfo_t *)bp->data);
    brelse(bp);

    /* 调用fat32_root_entry_init，获取到root根目录的fat_entry*/
    sb->fat32_sb_info.root_entry = fat32_root_entry_init(sb);
    sb->s_mount = sb->fat32_sb_info.root_entry;

    snprintf_test();
    printf_test();
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
    sb->fat32_sb_info.fatbase = fat_bpb->RsvdSecCnt;
    sb->fat32_sb_info.n_fats = fat_bpb->NumFATs;
    sb->fat32_sb_info.n_sectors_fat = fat_bpb->FATSz32;
    sb->fat32_sb_info.root_cluster_s = fat_bpb->RootClus;
    sb->fat32_sb_info.sector_per_cluster = fat_bpb->SecPerClus;
    sb->sector_size = fat_bpb->BytsPerSec;
    sb->n_sectors = fat_bpb->TotSec32;

    // repeat with sb->s_blocksize
    sb->fat32_sb_info.cluster_size = sb->sector_size * sb->fat32_sb_info.sector_per_cluster;
    sb->s_blocksize = sb->fat32_sb_info.cluster_size;

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

    Info("FilSysType : %s\n", fat_bpb->FilSysType);

    Info("BootCode : ");
    Show_bytes((byte_pointer)&fat_bpb->BootCode, sizeof(fat_bpb->BootCode));

    Info("BootSign : ");
    Show_bytes((byte_pointer)&fat_bpb->BootSign, sizeof(fat_bpb->BootSign));

    // panic("boot sector parser");
    return FR_OK;
}

int fat32_fcb_init(fat_entry_t *fat_ep_parent, uchar *long_name, uchar attr, char *fcb_char) {
    dirent_s_t dirent_s_cur;
    memset((void *)&dirent_s_cur, 0, sizeof(dirent_s_cur));
    // dirent_l_t dirent_l_cur;
    dirent_s_cur.DIR_Attr = attr;

    uint long_idx = -1;

    char file_name[NAME_LONG_MAX];
    char file_ext[4];

    /*short dirent*/
    int name_len = strlen(long_name);
    // 数据文件
    if (!DIR_BOOL(attr)) {
        if (str_split(long_name, '.', file_name, file_ext) == -1) {
            panic("fcb init : str split");
        }
        to_upper(file_ext);
        strncpy(dirent_s_cur.DIR_Name + 8, file_ext, 3); // extend name

        to_upper(file_name);
        strncpy(dirent_s_cur.DIR_Name, file_name, 8);
        if (strlen(long_name) > 8) {
            long_idx = fat32_find_same_name_cnt(fat_ep_parent, long_name);
            dirent_s_cur.DIR_Name[6] = '~';
            dirent_s_cur.DIR_Name[7] = long_idx;
        }
    } else {
        // 目录文件
        strncpy(file_name, long_name, 11);

        to_upper(file_name);
        strncpy(dirent_s_cur.DIR_Name, file_name, 11);
        if (strlen(long_name) > 8) {
            long_idx = fat32_find_same_name_cnt(fat_ep_parent, long_name);

            strncpy(dirent_s_cur.DIR_Name + 8, file_name + (name_len - 3), 3); // last three char
            dirent_s_cur.DIR_Name[6] = '~';
            dirent_s_cur.DIR_Name[7] = long_idx;
        }
    }
    dirent_s_cur.DIR_FileSize = 0;

    uint first_c = fat32_cluster_alloc(fat_ep_parent->fatfs_obj->dev);
    dirent_s_cur.DIR_FstClusHI = DIR_FIRST_HIGH(first_c);
    dirent_s_cur.DIR_FstClusLO = DIR_FIRST_LOW(first_c);

    /*push long dirent into stack*/
    Stack_t fcb_stack;
    stack_init(&fcb_stack);
    int iter_order = 1;
    uchar checksum = ChkSum(dirent_s_cur.DIR_Name);
    int char_idx = 0;
    // every long name entry
    for (int i = 1; i <= name_len / FAT_LFN_LENGTH + 1; i++) {
        dirent_l_t dirent_l_cur;
        memset((void *)&(dirent_l_cur), 0xFF, sizeof(dirent_l_cur));
        if (char_idx == name_len)
            break;

        // order
        if (i == name_len / FAT_LFN_LENGTH + 1)
            dirent_l_cur.LDIR_Ord = LAST_LONG_ENTRY_SET(i);
        else
            dirent_l_cur.LDIR_Ord = i;

        // Name
        int end_flag = 0;
        for (int i = 0; i < 5&&!end_flag; i++)
        {
            dirent_l_cur.LDIR_Name1[i] = LONG_NAME_CHAR_SET(long_name[char_idx]);                
            if (LONG_NAME_CHAR_VALID(long_name[char_idx++]))
                end_flag=1;
        }
        for (int i = 0; i < 6&&!end_flag; i++)
        {
            dirent_l_cur.LDIR_Name2[i] = LONG_NAME_CHAR_SET(long_name[char_idx]);                
            if (LONG_NAME_CHAR_VALID(long_name[char_idx++]))
                end_flag=1;
        }
        for (int i = 0; i < 2&&!end_flag; i++)
        {
            dirent_l_cur.LDIR_Name3[i] = LONG_NAME_CHAR_SET(long_name[char_idx]);                
            if (LONG_NAME_CHAR_VALID(long_name[char_idx++]))
                end_flag=1;
        }

        // Attr  and  Type
        dirent_l_cur.LDIR_Attr = ATTR_LONG_NAME_MASK;
        dirent_l_cur.LDIR_Type = 0;

        // check sum 
        dirent_l_cur.LDIR_Chksum = checksum;
        stack_push(&fcb_stack, dirent_l_cur);

        dirent_l_cur.LDIR_FstClusLO = 0;
    }

    while (!stack_is_empty(&fcb_stack)) {
        dirent_l_t fcb_l_tmp = stack_pop(&fcb_stack);
        memmove(fcb_char, &fcb_l_tmp, sizeof(fcb_l_tmp));
        fcb_char = fcb_char + sizeof(fcb_l_tmp);
    }
    memmove(fcb_char, &dirent_s_cur, sizeof(dirent_s_cur));
    return 1;
    // TODO: 获取当前时间和日期，还有TimeTenth
}

uchar ChkSum(uchar *pFcbName) {
    uchar Sum = 0;
    for (short FcbNameLen = 11; FcbNameLen != 0; FcbNameLen--) {
        Sum = ((Sum & 1) ? 0x80 : 0) + (Sum >> 1) + *pFcbName++;
    }
    return Sum;
}
