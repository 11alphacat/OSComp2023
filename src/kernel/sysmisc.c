#include "debug.h"
#include "common.h"
#include "proc/pcb_mm.h"
#include "kernel/trap.h"
#include "proc/sched.h"
#include "riscv.h"
#include "sbi.h"
#include "memory/buddy.h"
extern uint ticks;

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

    return ticks;
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

struct timespec {
    uint64 ts_sec;  /* Seconds */
    uint64 ts_nsec; /* Nanoseconds */
};

struct timeval {
    uint64 tv_sec;  /* Seconds */
    uint64 tv_usec; /* Microseconds */
};

#define FREQUENCY 12500000 // qemu时钟频率12500000
#define TIME2SEC(time) (time / FREQUENCY)
#define TIME2MS(time) (time * 1000 / FREQUENCY)
#define TIME2US(time) (time * 1000 * 1000 / FREQUENCY)
#define TIME2NS(time) (time * 1000 * 1000 * 1000 / FREQUENCY)

#define TIMESEPC2NS(sepc) (sepc.ts_nsec + sepc.ts_sec * 1000 * 1000 * 1000)
#define NS_to_S(ns) (ns / (1000 * 1000 * 1000))

#define TIME2TIMESPEC(time)                                                       \
    (struct timespec) {                                                           \
        .ts_sec = TIME2SEC(time), .ts_nsec = TIME2NS(time) % (1000 * 1000 * 1000) \
    }

#define TIME2TIMEVAL(time)                                                 \
    (struct timeval) {                                                     \
        .tv_sec = TIME2SEC(time), .tv_usec = TIME2US(time) % (1000 * 1000) \
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
    if (copyout(proc_current()->pagetable, addr, (char *)&tv_buf, sizeof(tv_buf)) < 0) {
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
    if (copyin(proc_current()->pagetable, (char *)&ts_buf, req, sizeof(ts_buf)) == -1) {
        return -1;
    }

    uint64 interval_ns = TIMESEPC2NS(ts_buf);
    uint64 time0 = TIME2NS(rdtime());

    acquire(&tickslock);
    while (TIME2NS(rdtime()) - time0 < interval_ns) {
        if (proc_killed(proc_current())) {
            release(&tickslock);
            return -1;
        }
        // sleep(&ticks, &tickslock);
        cond_wait(&cond_ticks, &tickslock);
    }
    release(&tickslock);

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

    if (copyout(proc_current()->pagetable, tp, (char *)&ts_buf, sizeof(ts_buf)) < 0) {
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

    if (copyout(proc_current()->pagetable, info, (char *)&info_buf, sizeof(info_buf)) < 0) {
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