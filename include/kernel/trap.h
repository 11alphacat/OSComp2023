#ifndef __TRAP_H__
#define __TRAP_H__

#include "common.h"

struct context;

// syscall.c
int argint(int, int *);
int argstr(int, char *, int);
int argaddr(int, uint64 *);
int arglong(int, uint64 *);
int fetchstr(uint64, char *, int);
int fetchaddr(uint64, uint64 *);
void syscall();

int copyout(pagetable_t, uint64, char *, uint64);
int copyin(pagetable_t, char *, uint64, uint64);
int copyinstr(pagetable_t, char *, uint64, uint64);

// trap.c
extern uint ticks;
extern struct spinlock tickslock;
void usertrapret(void);

void swtch(struct context *, struct context *);

#endif // __TRAP_H__