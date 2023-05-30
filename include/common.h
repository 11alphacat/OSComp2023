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
typedef unsigned long int nlink_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef long int off_t;
typedef long int blksize_t;
typedef long int blkcnt_t;

typedef unsigned short WORD;
typedef unsigned int DWORD;

typedef unsigned char *byte_pointer;
#define NULL ((void *)0)

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

#endif

#endif
