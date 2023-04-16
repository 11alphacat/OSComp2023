#ifndef __COMMON_H__
#define __COMMON_H__

// since xv6-user will use kernel header file, add this condition preprocess
#ifndef USER
// number of elements in fixed-size array
#define NELEM(x) (sizeof(x) / sizeof((x)[0]))

#define __user

typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long uint64;
typedef long int64;
typedef unsigned long uintptr_t;
typedef unsigned long size_t;
typedef long ssize_t;
typedef unsigned short mode_t;

typedef uint8 uint8_t;
typedef uint16 uint16_t;
typedef uint32 uint32_t;
typedef uint64 uint64_t;

typedef unsigned short WORD;
typedef unsigned int DWORD;

typedef unsigned char *byte_pointer;
#define NULL ((void *)0)

typedef uint64 pte_t;
typedef uint64 pde_t;
typedef uint64 *pagetable_t; // 512 PTEs

// math
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

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
int str_split(char *, char, char *, char *);

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
