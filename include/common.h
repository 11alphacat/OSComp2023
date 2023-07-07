#ifndef __COMMON_H__
#define __COMMON_H__

// since xv6-user will use kernel header file, add this condition preprocess
#ifndef USER

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x) / sizeof((x)[0]))

// common round_up/down
#define ROUND_UP(x, n) (((x) + (n)-1) & ~((n)-1))
#define ROUND_DOWN(x, n) ((x) & ~((n)-1))
#define DIV_ROUND_UP(n, d) (((n) + (d)-1) / (d))
#define __user

typedef enum { FD_NONE,
               FD_PIPE,
               FD_INODE,
               FD_DEVICE } type_t;

typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long uint64;
typedef long int64;
typedef int64 intptr_t;
typedef uint64 uintptr_t;
typedef unsigned long size_t;
typedef uint64 paddr_t;
typedef uint64 vaddr_t;
typedef long ssize_t;
typedef unsigned int mode_t;

// used in struct kstat
typedef unsigned long int dev_t;
typedef unsigned long int ino_t;
// typedef unsigned long int nlink_t;
typedef unsigned int nlink_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef long int off_t;
typedef long int blksize_t;
typedef long int blkcnt_t;

typedef int key_t;

typedef unsigned short WORD;
typedef unsigned int DWORD;

typedef unsigned char *byte_pointer;
#define NULL ((void *)0)

typedef uint64 sector_t;
typedef int pid_t;
typedef int tid_t;
typedef int pgrp_t;
typedef uint64 pte_t;
typedef uint64 pde_t;
typedef uint64 *pagetable_t; // 512 PTEs

// remember return to fat32_file.h
struct devsw {
    int (*read)(int, uint64, int);
    int (*write)(int, uint64, int);
};
extern struct devsw devsw[];

// math
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define CEIL_DIVIDE(x, y) (((x) + (y)-1) / (y))
// util

/* Character code support macros */
#define IsUpper(c) ((c) >= 'A' && (c) <= 'Z')
#define IsLower(c) ((c) >= 'a' && (c) <= 'z')
#define IsDigit(c) ((c) >= '0' && (c) <= '9')

// time
#define FREQUENCY 12500000 // qemu时钟频率12500000
#define TIME2SEC(time) (time / FREQUENCY)
#define TIME2MS(time) (time * 1000 / FREQUENCY)
#define TIME2US(time) (time * 1000 * 1000 / FREQUENCY)
#define TIME2NS(time) (time * 1000 * 1000 * 1000 / FREQUENCY)

#define TIMESEPC2NS(sepc) (sepc.ts_nsec + sepc.ts_sec * 1000 * 1000 * 1000)
#define NS_to_S(ns) (ns / (1000 * 1000 * 1000))
#define S_to_NS(s) (s * 1UL * 1000 * 1000 * 1000)

#define TIME2TIMESPEC(time)                                                       \
    (struct timespec) {                                                           \
        .ts_sec = TIME2SEC(time), .ts_nsec = TIME2NS(time) % (1000 * 1000 * 1000) \
    }

#define TIME2TIMEVAL(time)                                                 \
    (struct timeval) {                                                     \
        .tv_sec = TIME2SEC(time), .tv_usec = TIME2US(time) % (1000 * 1000) \
    }
struct timespec {
    uint64 ts_sec;  /* Seconds */
    uint64 ts_nsec; /* Nanoseconds */
};

struct timeval {
    uint64 tv_sec;  /* Seconds */
    uint64 tv_usec; /* Microseconds */
};
typedef int64 s64;
typedef uint64 u64;
typedef s64 ktime_t;
#define NSEC_PER_SEC 1000000000L
#define KTIME_MAX ((s64) ~((u64)1 << 63))
#define KTIME_SEC_MAX (KTIME_MAX / NSEC_PER_SEC)
static inline ktime_t ktime_set(const s64 secs, const uint64 nsecs) {
    if (unlikely(secs >= KTIME_SEC_MAX))
        return KTIME_MAX;
    return secs * NSEC_PER_SEC + (s64)nsecs;
}
static inline int should_fail_futex(int fshared) {
    return 0;
}

static inline int timespec64_valid(const struct timespec *ts) {
    /* Dates before 1970 are bogus */
    if (ts->ts_sec < 0)
        return 0;
    /* Can't have more nanoseconds then a second */
    if ((uint64)ts->ts_nsec >= NSEC_PER_SEC)
        return 0;
    return 1;
}

static inline ktime_t timespec64_to_ktime(struct timespec ts) {
    return ktime_set(ts.ts_sec, ts.ts_nsec);
}
// string.c
int memcmp(const void *, const void *, uint);
void *memmove(void *, const void *, uint);
void *memset(void *, int, uint);
char *safestrcpy(char *, const char *, int);
int strlen(const char *);
size_t strnlen(const char *s, size_t count);
int strncmp(const char *, const char *, uint);
char *strncpy(char *, const char *, int);
void str_toupper(char *);
void str_tolower(char *);
char *strchr(const char *, int);
int str_split(const char *, const char, char *, char *);
char *strcat(char *dest, const char *src);
char *strstr(const char *haystack, const char *needle);
int strcmp(const char *l, const char *r);
int is_suffix(const char *str, const char *suffix);

// printf.c
void printf(char *, ...);
void panic(char *) __attribute__((noreturn));
void Show_bytes(byte_pointer, int);
void printf_bin(uchar *, int);

// sprintf.c
int sprintf(char *buf, const char *fmt, ...);
int snprintf(char *buf, int size, const char *fmt, ...);

#define UINT64_MAX 0xFFFFFFFFFFFFFFFF

#endif

#endif
