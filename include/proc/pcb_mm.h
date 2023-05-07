#ifndef __PCB_MM_H__
#define __PCB_MM_H__
#include "common.h"
#include "proc/pcb_life.h"

void proc_mapstacks(pagetable_t);
pagetable_t proc_pagetable(struct proc *);
void proc_freepagetable(pagetable_t, uint64);
int growproc(int);

#endif