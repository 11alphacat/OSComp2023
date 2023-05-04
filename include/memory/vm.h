#ifndef __VM_H__
#define __VM_H__

#include "common.h"
#define COMMONPAGE 0
#define SUPERPAGE 1 /* 2MB superpage */

void kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm);
int walk(pagetable_t pagetable, uint64 va, int alloc, int lowlevel, pte_t **pte);
uint64 walkaddr(pagetable_t pagetable, uint64 va);
int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm, int lowlevel);
pagetable_t uvmcreate(void);
uint64 uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm);
uint64 uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz);
int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz);
void uvmfree(pagetable_t pagetable, uint64 sz);
void uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free, int on_demand);
void uvmclear(pagetable_t pagetable, uint64 va);

/* copy-on write */
int cow(pagetable_t pagetable, uint64 stval);

/* print the pagetable */
void vmprint(pagetable_t pagetable, int isroot, int level, int single, uint64 vabase);

#endif // __VM_H__