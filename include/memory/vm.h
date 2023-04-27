#ifndef __VM_H__
#define __VM_H__

#include "common.h"
#define MAPPAGE 0
#define MAPSUPERPAGE 1

void kvmmap(pagetable_t, uint64, uint64, uint64, int);
int walk(pagetable_t, uint64, int, int, pte_t **);
uint64 walkaddr(pagetable_t, uint64);
int mappages(pagetable_t, uint64, uint64, uint64, int, int);
pagetable_t uvmcreate(void);
uint64 uvmalloc(pagetable_t, uint64, uint64, int);
uint64 uvmdealloc(pagetable_t, uint64, uint64);
int uvmcopy(pagetable_t, pagetable_t, uint64);
void uvmfree(pagetable_t, uint64);
void uvmunmap(pagetable_t, uint64, uint64, int);
void uvmclear(pagetable_t, uint64);

// print the pagetable
void vmprint(pagetable_t pagetable, int isroot, int level, int single, uint64 vabase);

#endif // __VM_H__