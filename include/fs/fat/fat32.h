#ifndef __FAT_FAT32_H__
#define __FAT_FAT32_H__

#include "common.h"
#include "atomic/spinlock.h"

/*
    FAT32
    +---------------------------+
    |       Boot Sector         |
    +---------------------------+
    |         FsInfo            |
    +---------------------------+
    |     FAT Region (FAT1)     |
    +---------------------------+
    |     FAT Region (FAT2)     |
    +---------------------------+
    |     Backup BPB Structure  |
    +---------------------------+
    |    Root Directory Region  |
    +---------------------------+
    |    Data Region (Cluster 2)|
    +---------------------------+
    |    Data Region (Cluster 3)|
    +---------------------------+
    |           ...             |
    +---------------------------+
    |    Data Region (Cluster n)|
    +---------------------------+
*/

/* Character code support macros */
#define IsUpper(c) ((c) >= 'A' && (c) <= 'Z')
#define IsLower(c) ((c) >= 'a' && (c) <= 'z')
#define IsDigit(c) ((c) >= '0' && (c) <= '9')

/*sector size*/
#define FF_MAX_SS 512

/* File access mode and open method flags (3rd argument of f_open) */
#define FA_READ 0x01
#define FA_WRITE 0x02
#define FA_OPEN_EXISTING 0x00
#define FA_CREATE_NEW 0x04
#define FA_CREATE_ALWAYS 0x08
#define FA_OPEN_ALWAYS 0x10
#define FA_OPEN_APPEND 0x30

/* File attribute bits for directory entry (FILINFO.fattrib) */
#define ATTR_READ_ONLY 0x01 /* Read only */
#define ATTR_HIDDEN 0x02    /* Hidden */
#define ATTR_SYSTEM 0x04    /* System */
#define ATTR_VOLUME_ID 0x10 /* Directory */
#define ATTR_ARCHIVE 0x20   /* Archive */

/*BPB (BIOS Parameter Block)*/
#define BS_JmpBoot 0      /* jump instruction (3-byte) */
#define BS_OEMName 3      /* OEM name (8-byte) */
#define BPB_BytsPerSec 11 /* Sector size [byte] (WORD) */
#define BPB_SecPerClus 13 /* Cluster size [sector] (BYTE) */
#define BPB_RsvdSecCnt 14 /* Size of reserved area [sector] (WORD) */
#define BPB_NumFATs 16    /* Number of FATs (BYTE) */
#define BPB_RootEntCnt 17 /* Size of root directory area for FAT [entry] (WORD) */
#define BPB_TotSec16 19   /* Volume size (16-bit) [sector] (WORD) */
#define BPB_Media 21      /* Media descriptor byte (BYTE) */
#define BPB_FATSz16 22    /* FAT size (16-bit) [sector] (WORD) */
#define BPB_SecPerTrk 24  /* Number of sectors per track for int13h [sector] (WORD) */
#define BPB_NumHeads 26   /* Number of heads for int13h (WORD) */
#define BPB_HiddSec 28    /* Volume offset from top of the drive (DWORD) */
#define BPB_TotSec 32     /* Volume size (32-bit) [sector] (DWORD) */

/*Extended Boot Record*/
#define BPB_FATSz 36     /* FAT32: FAT size [sector] (DWORD) */
#define BPB_ExtFlags 40  /* FAT32: Extended flags (WORD) */
#define BPB_FSVer 42     /* FAT32: Filesystem version (WORD) */
#define BPB_RootClus 44  /* FAT32: Root directory cluster (DWORD) */
#define BPB_FSInfo 48    /* FAT32: Offset of FSINFO sector (WORD) */
#define BPB_BkBootSec 50 /* FAT32: Offset of backup boot sector (WORD) */
#define BPB_Reserved 52  /* FAT32: Reserved. Must be set to 0x0.*/

#define BS_DrvNum 64     /* FAT32: Physical drive number for int13h (BYTE) */
#define BS_NTres 65      /* FAT32: Error flag (BYTE) */
#define BS_BootSig 66    /* FAT32: Extended boot signature (BYTE) */
#define BS_VolID 67      /* FAT32: Volume serial number (DWORD) */
#define BS_VolLab 71     /* FAT32: Volume label string (8-byte) */
#define BS_FilSysType 82 /* FAT32: Filesystem type string (8-byte) */
#define BS_BootCode 90   /* FAT32: Boot code (420-byte) */
#define BS_BootSign 510  /* Signature word (WORD) */

/*FSInfo Structure (FAT32 only)*/
#define FSI_LeadSig 0      /* FAT32 FSI: Leading signature (DWORD) */
#define FSI_Reserved1 4    /* FAT32 FSI: Reserved1. This field should be always initialized to zero.*/
#define FSI_StrucSig 484   /* FAT32 FSI: Structure signature (DWORD) */
#define FSI_Free_Count 488 /* FAT32 FSI: Number of free clusters (DWORD) */
#define FSI_Nxt_Free 492   /* FAT32 FSI: Last allocated cluster (DWORD) */
#define FSI_Reserved2 496  /* FAT32 FSI: Reserved. This field should be always initialized to zero.*/
#define FSI_TrailSig 512   /* FAT32 FSI: */

#define FAT_SFN_LENGTH 11 // 短文件名长度
#define FAT_LFN_LENGTH 13 // 长文件名长度

// FAT32 Boot Record
struct FAT32_BootRecord {
    /*FAT common field*/

    uchar Jmpboot[3]; // Jump instruction to boot code.
    // 0xEB 0x?? 0x??
    uchar OEMName[8]; // OEM Name Identifier.
    // Can be set by a FAT implementation to any desired value.
    uchar BytsPerSec[2]; // Count of bytes per sector(*)
    // 512 1024 2048 4096
    uchar SecPerClus[1]; // Number of sectors per allocation unit.
    // 1 2 4 8 16 32 64 128
    uchar RsvdSecCnt[2]; // Number of reserved sectors in the reserved region
    // of the volume starting at the first sector of the volume.
    uchar NumFATs[1]; // The count of file allocation tables (FATs) on the volume.
    // 1 or 2
    uchar RootEntCnt[2]; // for FAT32 , set to 0
    uchar TotSec16[2];
    uchar Media[1];
    uchar FATSz16[2];
    uchar SecPerTrk[2];
    uchar NumHeads[2];
    uchar HiddSecp[4];
    uchar TotSec32[4];

    /*special for FAT32*/
    uchar FATSz32[4];
    uchar ExtFlags[2];
    uchar FSVer[2];
    uchar RootClus[4];
    uchar FSInfo[2];
    uchar BkBootSec[2];
    uchar Reserved1[12];
    uchar DrvNum[1];
    uchar Reserved2[1];
    uchar BootSig[1];
    uchar VolID[4];
    uchar VolLab[11];
    uchar FilSysType[8];
    uchar BootCode32[420];
    uchar BootSign[2];
} __attribute__((packed));

typedef struct FAT32_BootRecord fat_bpb_t;

// FAT32 Fsinfo
struct FAT32_Fsinfo {
    uchar LeadSig[4];
    uchar Reserved1[480];
    uchar StrucSig[4];
    uchar Free_Count[4];
    uchar Nxt_Free[4];
    uchar Reserved2[12];
    uchar TrailSig[4];
} __attribute__((packed));

typedef struct FAT32_Fsinfo fsinfo_t;

// Date
typedef struct __date_t {
    uchar day : 5;   // 0~31
    uchar month : 4; // Jan ~ Dec
    uchar year : 7;  // form 1980 (1980~2107)
} FAT_date_t;

// Time
typedef struct __time_t {
    uchar second_per_2 : 5; // 2-second increments 0~59
    uchar minute : 6;       // number of minutes 0~59
    uchar hour : 5;         // hours 0~23
} FAT_time_t;

// Directory Structure (short name)
struct Short_Dir_t {
    uchar DIR_Name[FAT_SFN_LENGTH]; // directory name
    uchar DIR_Attr[1];              // directory attribute
    uchar DIR_NTRes[1];             // reserved
    uchar DIR_CrtTimeTenth[1];      // create time
    // Count of tenths of a second
    // 0 <= DIR_CrtTimeTenth <= 199
    FAT_time_t DIR_CrtTime;    // create time, 2 bytes
    FAT_date_t DIR_CrtDate;    // create date, 2 bytes
    FAT_time_t DIR_LstAccDate; // last access date, 2 bytes
    uchar DIR_FstClusHI[2];    // High word of first data cluster number
    uchar DIR_WrtTime[2];      // Last modification (write) time.
    uchar DIR_WrtDate[2];      // Last modification (write) date.
    uchar DIR_FstClusLO[2];    // Low word of first data cluster number
    uchar DIR_FileSize[4];     // 32-bit quantity containing size
} __attribute__((packed));
typedef struct Short_Dir_t dirent_s_t;

// Directory Structure (long name)
struct Long_Dir_t {
    uchar LDIR_Ord[1];       // The order of this entry in the sequence
    uchar LDIR_Name1[10];    // characters 1 through 5
    uchar LDIR_Attr[1];      // Attributes
    uchar LDIR_Type[1];      // Must be set to 0.
    uchar LDIR_Chksum[1];    // Checksum of name
    uchar LDIR_Name2[12];    // characters 6 through 11
    uchar LDIR_FstClusLO[2]; // Must be set to 0
    uchar LDIR_Name3[4];     // characters 12 and 13
} __attribute__((packed));
typedef struct Long_Dirt_t dirent_l_t;

/* File function return code (FRESULT) */
typedef enum {
    FR_OK = 0,              /* (0) Succeeded */
    FR_DISK_ERR,            /* (1) A hard error occurred in the low level disk I/O layer */
    FR_INT_ERR,             /* (2) Assertion failed */
    FR_NOT_READY,           /* (3) The physical drive cannot work */
    FR_NO_FILE,             /* (4) Could not find the file */
    FR_NO_PATH,             /* (5) Could not find the path */
    FR_INVALID_NAME,        /* (6) The path name format is invalid */
    FR_DENIED,              /* (7) Access denied due to prohibited access or directory full */
    FR_EXIST,               /* (8) Access denied due to prohibited access */
    FR_INVALID_OBJECT,      /* (9) The file/directory object is invalid */
    FR_WRITE_PROTECTED,     /* (10) The physical drive is write protected */
    FR_INVALID_DRIVE,       /* (11) The logical drive number is invalid */
    FR_NOT_ENABLED,         /* (12) The volume has no work area */
    FR_NO_FILESYSTEM,       /* (13) There is no valid FAT volume */
    FR_MKFS_ABORTED,        /* (14) The f_mkfs() aborted due to any problem */
    FR_TIMEOUT,             /* (15) Could not get a grant to access the volume within defined period */
    FR_LOCKED,              /* (16) The operation is rejected according to the file sharing policy */
    FR_NOT_ENOUGH_CORE,     /* (17) LFN working buffer could not be allocated */
    FR_TOO_MANY_OPEN_FILES, /* (18) Number of open files > FF_FS_LOCK */
    FR_INVALID_PARAMETER    /* (19) Given parameter is invalid */
} FRESULT;

struct FATFS {
    uint dev; // device number
    struct spinlock lock;

    uchar n_fats;       // Number of FATs (1 or 2)
    uint n_sectors_fat; // Number of sectors per FAT

    ushort volbase; // Volume base sector
    uint dirbase;   // Root directory base sector
    uint fatbase;   // FAT base sector
    uint database;  // Data base sector

    uint sector_size;        // size of a sector
    uint cluster_size;       // size of a cluster
    uint sector_per_cluster; // sector of a cluster

    struct FAT32_Fsinfo fsinfo;
    /*access window*/
    uint winsect;
    uchar win[FF_MAX_SS];
};

typedef struct FATFS FATFS_t;

#define FAT_N(fatfs) (fatfs->n_fats)
#define FAT_SECTOR_N(fatfs) (fatfs->n_sectors_fat)
#define FAT_BASE(fatfs) (fatfs->fatbase)

#define BPS(fatfs) (fatfs->sector_size)
#define BPC(fatfs) (fatfs->cluster_size)
#define SPC(fatfs) (fatfs->sector_per_cluster)

FRESULT fat32_mount(int, FATFS_t *fatfs);
FRESULT fat32_mkfs(void);
FRESULT fat32_open(void);
FRESULT fat32_close(void);

int fat32_boot_sector_parser(FATFS_t *, fat_bpb_t *);

#endif // __FAT_FAT32_H__
