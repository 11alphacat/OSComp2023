#ifndef __TRAP_H__
#define __TRAP_H__

#include "common.h"

#define INSTUCTION_PAGEFAULT 12
#define LOAD_PAGEFAULT 13
#define STORE_PAGEFAULT 15

struct context;
struct _file;

// syscall.c
int argint(int, int *);
void argulong(int, unsigned long *);
void arglong(int, long *);
int argfd(int n, int *pfd, struct _file **pf);
int argstr(int, char *, int);
int argaddr(int, uint64 *);
// int arglong(int, uint64 *);
int fetchstr(uint64, char *, int);
int fetchaddr(uint64, uint64 *);
void syscall();

int copyout(pagetable_t, uint64, char *, uint64);
int copyin(pagetable_t, char *, uint64, uint64);
int copyinstr(pagetable_t, char *, uint64, uint64);
int either_copyout(int user_dst, uint64 dst, void *src, uint64 len);
int either_copyin(void *dst, int user_src, uint64 src, uint64 len);

// trap.c
extern uint ticks;
extern struct spinlock tickslock;
void usertrapret(void);

void swtch(struct context *, struct context *);

// pagefault.c
int pagefault(uint64 cause, pagetable_t pagetable, vaddr_t stval);
#endif // __TRAP_H__