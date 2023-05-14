#include "debug.h"
#include "common.h"
#include "proc/pcb_mm.h"
#include "kernel/trap.h"
#include "proc/sched.h"
#include "sbi.h"
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
    "www.hdu.lostwakeup.edu.cn"};

/*
 * 功能：获取进程时间；
 * 输入：tms结构体指针，用于获取保存当前进程的运行时间数据；
 * 返回值：成功返回已经过去的滴答数，失败返回-1;
 */
// struct tms *tms;s
uint64 sys_times(void) {
    uint64 addr;
    argaddr(0, &addr);

    struct proc *p = current();
    struct tms tms_buf;
    tms_buf.tms_stime = p->tms_stime;
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
    yield();
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

#define TICK_GRANULARITY 10L

#define TICK2SEC(tick) (tick / TICK_GRANULARITY)
#define TICK2MS(tick) (tick / TICK_GRANULARITY * 1000)
#define TICK2US(tick) (tick / TICK_GRANULARITY * 1000 * 1000)
#define TICK2NS(tick) (tick / TICK_GRANULARITY * 1000 * 1000 * 1000)

#define SEC2TICK(sec) (sec * TICK_GRANULARITY)
#define NS2TICK(ns) (ns * (TICK_GRANULARITY) / (1000 * 1000 * 1000))

#define TICK2TIMESPEC(tick)                                                       \
    (struct timespec) {                                                           \
        .ts_sec = TICK2SEC(tick), .ts_nsec = TICK2NS(tick) % (1000 * 1000 * 1000) \
    }

#define TICK2TIMEVAL(tick)                                                 \
    (struct timeval) {                                                     \
        .tv_sec = TICK2SEC(tick), .tv_usec = TICK2US(tick) % (1000 * 1000) \
    }

#define SEPC2NS(sepc) (sepc.ts_nsec + sepc.ts_sec * 1000 * 1000 * 1000)

/*
 * 功能：获取时间；
 * 输入： timespec结构体指针用于获得时间值；
 * 返回值：成功返回0，失败返回-1;
 */
// struct timespec *ts;
uint64 sys_gettimeofday(void) {
    uint64 addr;
    argaddr(0, &addr);
    acquire(&tickslock);
    struct timespec ts_buf = TICK2TIMESPEC(ticks);
    release(&tickslock);
    if (either_copyout(1, addr, &ts_buf, sizeof(ts_buf)) == -1)
        return -1;
    return 0;
}

/*
 * 功能：执行线程睡眠，sleep()库函数基于此系统调用；
 * 输入：睡眠的时间间隔；
 */
// struct timespec {
// 	time_t tv_sec;        /* 秒 */
// 	long   tv_nsec;       /* 纳秒, 范围在0~999999999 */
// };
// 返回值：成功返回0，失败返回-1;
// const struct timespec *req, struct timespec *rem;
uint64 sys_nanosleep(void) {
    uint64 addr1, addr2;
    argaddr(0, &addr1);
    argaddr(1, &addr2);

    struct timespec ts_buf;
    if (either_copyin(&ts_buf, 1, addr1, sizeof(ts_buf)) == -1)
        return -1;

    uint ticks0;
    uint interval_ns = SEPC2NS(ts_buf);
    uint interval_tick = NS2TICK(interval_ns);

#ifdef __DEBUG_PROC__
    printfYELLOW("sleep : pid %d sleep(%d)s start...\n", current()->pid, interval_tick / 10); // debug
#endif

    acquire(&tickslock);
    ticks0 = ticks;
    while (ticks - ticks0 < interval_tick) {
        if (killed(current())) {
            release(&tickslock);
            return -1;
        }
        sleep(&ticks, &tickslock);
    }
    release(&tickslock);

#ifdef __DEBUG_PROC__
    printfYELLOW("sleep : pid %d sleep(%d)s over...", current()->pid, interval_tick / 10); // debug
#endif

    return 0;
}

uint64 sys_shutdown() {
    sbi_shutdown();
    panic("shutdown: can not reach here");
}