#include "debug.h"
#include "common.h"
#include "proc/pcb_mm.h"
#include "kernel/trap.h"
#include "proc/sched.h"
#include "lib/riscv.h"
#include "lib/sbi.h"
#include "memory/buddy.h"
#include "debug.h"
#include "fs/vfs/fs.h"
#include "fs/bio.h"
#include "kernel/syscall.h"
#include "lib/ctype.h"
extern atomic_t ticks;

struct tms {
    long tms_utime;
    long tms_stime;
    long tms_cutime;
    long tms_cstime;
};

struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

struct utsname sys_ut = {
    "LostWakeup OS",
    "oskernel",
    "1.0.0-1-generic",
    "0.0.0",
    "RISCV",
    "www.hdu.lostwakeup.edu.cn",
};

/*
 * 功能：获取进程时间；
 * 输入：tms结构体指针，用于获取保存当前进程的运行时间数据；
 * 返回值：成功返回已经过去的滴答数，失败返回-1;
 */
// struct tms *tms;s
uint64 sys_times(void) {
    uint64 addr;
    argaddr(0, &addr);

    // struct proc *p = proc_current();
    struct tms tms_buf;
    // tms_buf.tms_stime = p->tms_stime;

    tms_buf.tms_stime = 0;
    tms_buf.tms_utime = 0;
    tms_buf.tms_cstime = 0;
    tms_buf.tms_cutime = 0;

    // TODO: utime and stime
    if (either_copyout(1, addr, &tms_buf, sizeof(tms_buf)) == -1)
        return -1;

    return atomic_read(&ticks);
}
/*
 * 功能：打印系统信息；
 * 输入：utsname结构体指针用于获得系统信息数据；
 * 返回值：成功返回0，失败返回-1;
 */
// struct utsname *uts;
uint64 sys_uname(void) {
    uint64 addr;
    uint64 ret = 0;
    argaddr(0, &addr);

    if (either_copyout(1, addr, &sys_ut, sizeof(sys_ut)) == -1)
        ret = -1;

    return ret;
}

/*
 * 功能：让出调度器；
 * 输入：系统调用ID；
 * 返回值：成功返回0，失败返回-1;
 */
//
uint64 sys_sched_yield(void) {
    thread_yield();
    return 0;
}

/*
 * 功能：获取时间；
 * 输入： timespec结构体指针用于获得时间值；
 * int gettimeofday(struct timeval *tv, struct timezone *tz);
 * 返回值：成功返回0，失败返回-1;
 */
uint64 sys_gettimeofday(void) {
    uint64 addr;
    argaddr(0, &addr);
    uint64 time = rdtime();
    struct timeval tv_buf = TIME2TIMEVAL(time);
    // Log("%ld", tv_buf.tv_sec);
    // Log("%ld", tv_buf.tv_usec);
    if (copyout(proc_current()->mm->pagetable, addr, (char *)&tv_buf, sizeof(tv_buf)) < 0) {
        return -1;
    }
    return 0;
}

/*
 * 功能：执行线程睡眠，sleep()库函数基于此系统调用；
 * 输入：睡眠的时间间隔；
 * int nanosleep(const struct timespec *req, struct timespec *rem);
 * 返回值：成功返回0，失败返回-1;
 */
uint64 sys_nanosleep(void) {
    /* NOTE:currently, we do not support rem! */
    uint64 req;
    argaddr(0, &req);

    struct timespec ts_buf;
    if (copyin(proc_current()->mm->pagetable, (char *)&ts_buf, req, sizeof(ts_buf)) == -1) {
        return -1;
    }

    do_sleep_ns(thread_current(), ts_buf);
    return 0;
}

uint64 sys_shutdown() {
    sbi_shutdown();
    panic("shutdown: can not reach here");
}

typedef int clockid_t;
#define CLOCK_REALTIME 0
// int clock_gettime(clockid_t clockid, struct timespec *tp);
uint64 sys_clock_gettime(void) {
    int clockid;
    uint64 tp;
    struct timespec ts_buf;
    argint(0, &clockid);
    argaddr(1, &tp);
    uint64 time = rdtime();
    ts_buf = TIME2TIMESPEC(time);

    if (copyout(proc_current()->mm->pagetable, tp, (char *)&ts_buf, sizeof(ts_buf)) < 0) {
        return -1;
    }

    return 0;
}

struct sysinfo {
    long uptime; /* Seconds since boot */
    // unsigned long loads[3];  /* 1, 5, and 15 minute load averages */
    unsigned long totalram; /* Total usable main memory size */
    unsigned long freeram;  /* Available memory size */
    // unsigned long sharedram; /* Amount of shared memory */
    // unsigned long bufferram; /* Memory used by buffers */
    // unsigned long totalswap; /* Total swap space size */
    // unsigned long freeswap;  /* Swap space still available */
    unsigned short procs; /* Number of current processes */
    // unsigned long totalhigh; /* Total high memory size */
    // unsigned long freehigh;  /* Available high memory size */
    // unsigned int mem_unit;   /* Memory unit size in bytes */
    // char _f[20-2*sizeof(long)-sizeof(int)]; /* Padding to 64 bytes */
};

// int sysinfo(struct sysinfo *info);
uint64 sys_sysinfo(void) {
    uint64 info;
    argaddr(0, &info);

    long time_s = TIME2SEC(rdtime());
    struct sysinfo info_buf;
    info_buf.uptime = time_s;
    info_buf.freeram = get_free_mem();
    info_buf.totalram = PHYSTOP - START_MEM; // 120MB
    info_buf.procs = get_current_procs();

    if (copyout(proc_current()->mm->pagetable, info, (char *)&info_buf, sizeof(info_buf)) < 0) {
        return -1;
    }

    return 0;
}

// void syslog(int priority, const char *format, ...);
uint64 sys_syslog(void) {
    int priority;
    // char* format;
    argint(0, &priority);
    // argaddr(1, format);
    // Log(format);

    return 0;
}

/* inefficient, use for debug only! */
static void print_rawstr(const char *c, size_t len) {
    for (int i = 0; i < len; i++) {
        if (*(c + i) == ' ') {
            printf(" ");
        } else if (isalnum(*(c + i)) || *(c + i) == '.' || *(c + i) == '~' || *(c + i) == '_') {
            printf("%c", *(c + i));
        } else {
            printf(" ");
        }
    }
}

static void print_short_dir(const dirent_s_t *buf) {
    printf("(short) ");
    printf("name: ", buf->DIR_Name);
    print_rawstr((char *)buf->DIR_Name, FAT_SFN_LENGTH);
    printf("  ");
    printf("attr: %#x\t", buf->DIR_Attr);
    printf("dev: %d\t", buf->DIR_Dev);
    printf("filesize %d\n", buf->DIR_FileSize);
    // printf("create_time_tenth %d\t", buf->DIR_CrtTimeTenth);
    // printf("create_time %d\n", buf->DIR_CrtTime);
    // printf("crea");
}

static void print_long_dir(const dirent_l_t *buf) {
    printf("(long) ");
    printf("name1: ");
    print_rawstr((char *)buf->LDIR_Name1, 10);
    printf("  ");
    printf("name2: ");
    print_rawstr((char *)buf->LDIR_Name2, 12);
    printf("  ");
    printf("name3: ");
    print_rawstr((char *)buf->LDIR_Name3, 4);
    printf("  ");
    printf("attr %#x\n", buf->LDIR_Attr);
}

void sys_print_rawfile(void) {
    int fd;
    int printdir;
    struct file *f;

    printfGreen("==============\n");
    if (argfd(0, &fd, &f) < 0) {
        printfGreen("file doesn't open!\n");
        return;
    }
    argint(1, &printdir);

    ASSERT(f->f_type == FD_INODE);
    int cnt = 0;
    int pos = 0;
    uint64 iter_c_n = f->f_tp.f_inode->fat32_i.cluster_start;

    printfGreen("fd is %d\n", fd);
    struct inode *ip = f->f_tp.f_inode;
    if (!S_ISDIR(ip->i_mode) && printdir == 1) {
        printfGreen("the file is not a directory\n");
        return;
    }
    int off = 0;
    // print logistic clu.no and address(in fat32.img)
    while (!ISEOF(iter_c_n)) {
        uint64 addr = (iter_c_n - fat32_sb.fat32_sb_info.root_cluster_s) * __BPB_BytsPerSec * __BPB_SecPerClus + FSIMG_STARTADDR;
        printfGreen("cluster no: %d\t address: %p \toffset %d(%#p)\n", cnt++, addr, pos, pos);
        if (printdir == 1) {
            int first_sector = FirstSectorofCluster(iter_c_n);
            int init_s_n = LOGISTIC_S_NUM(pos);
            struct buffer_head *bp;
            for (int s = init_s_n; s < __BPB_SecPerClus; s++) {
                bp = bread(ip->i_dev, first_sector + s);
                for (int i = 0; i < 512 && i < ip->i_size; i += 32) {
                    dirent_s_t *tmp = (dirent_s_t *)(bp->data + i);
                    printf("%x ", off++);
                    if (LONG_NAME_BOOL(tmp->DIR_Attr)) {
                        print_long_dir((dirent_l_t *)(bp->data + i));
                    } else {
                        print_short_dir((dirent_s_t *)(bp->data + i));
                    }
                }
                printfRed("===Sector %d end===\n", s);
                brelse(bp);
            }
        }
        pos += __BPB_BytsPerSec * __BPB_SecPerClus;
        iter_c_n = fat32_next_cluster(iter_c_n);
    }
    printfGreen("file size is %d(%#p)\n", f->f_tp.f_inode->i_size, f->f_tp.f_inode->i_size);
    printfGreen("==============\n");
    return;
}
