#ifndef __PCB_MM_H__
#define __PCB_MM_H__
#include "common.h"
#include "proc/pcb_life.h"

void tcb_mapstacks(pagetable_t);
pagetable_t proc_pagetable(struct proc *);
int thread_trapframe(struct tcb *t, int still);
void proc_freepagetable(pagetable_t pagetable, uint64 sz, int maxoffset);
int growproc(int);

#endif