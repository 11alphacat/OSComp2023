#include "common.h"
#include "riscv.h"
#include "proc/pcb_mm.h"
#include "proc/pcb_life.h"
#include "kernel/trap.h"
#include "memory/memlayout.h"
#include "memory/allocator.h"
#include "memory/vm.h"
#include "debug.h"

extern struct proc proc[NPROC];
extern char trampoline[]; // trampoline.S

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void proc_mapstacks(pagetable_t kpgtbl) {
    struct proc *p;

    for (p = proc; p < &proc[NPROC]; p++) {
        char *pa = kalloc();
        if (pa == 0)
            panic("kalloc");
        uint64 va = KSTACK((int)(p - proc));
        kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
    }
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p) {
    pagetable_t pagetable;

    // An empty page table.
    pagetable = uvmcreate();
    if (pagetable == 0)
        return 0;

    // map the trampoline code (for system call return)
    // at the highest user virtual address.
    // only the supervisor uses it, on the way
    // to/from user space, so not PTE_U.
    if (mappages(pagetable, TRAMPOLINE, PGSIZE,
                 (uint64)trampoline, PTE_R | PTE_X, 0)
        < 0) {
        uvmfree(pagetable, 0);
        return 0;
    }

    // map the trapframe page just below the trampoline page, for
    // trampoline.S.
    if (mappages(pagetable, TRAPFRAME, PGSIZE,
                 (uint64)(p->trapframe), PTE_R | PTE_W, 0)
        < 0) {
        uvmunmap(pagetable, TRAMPOLINE, 1, 0, 0);
        uvmfree(pagetable, 0);
        return 0;
    }

    return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void proc_freepagetable(pagetable_t pagetable, uint64 sz) {
    uvmunmap(pagetable, TRAMPOLINE, 1, 0, 0);
    uvmunmap(pagetable, TRAPFRAME, 1, 0, 0);
    uvmfree(pagetable, sz);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n) {
    uint64 oldsz, newsz, sz;
    struct proc *p = myproc();
    sz = p->sz;
    oldsz = p->sz;
    newsz = p->sz + n;

    if (n > 0) {
        pte_t *pte;
        int level = walk(p->pagetable, oldsz, 0, 0, &pte);
        if (pte == NULL && level != -1) {
            panic("growproc: wrong");
        }
        ASSERT(level <= 1);
        if (level == COMMONPAGE) {
            if (PGROUNDUP(oldsz) >= newsz) {
                p->sz = newsz;
                return 0;
            }
        } else if (level == SUPERPAGE) {
            if (SUPERPG_ROUNDUP(oldsz) >= newsz) {
                p->sz = newsz;
                return 0;
            }
        } else if (level != -1) {
            panic("growproc: wrong level");
        }

        if ((sz = uvmalloc(p->pagetable, oldsz, newsz, PTE_W)) == 0) {
            return -1;
        }
    } else if (n < 0) {
        sz = uvmdealloc(p->pagetable, oldsz, newsz);
    }
    p->sz = sz;
    return 0;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int either_copyout(int user_dst, uint64 dst, void *src, uint64 len) {
    struct proc *p = myproc();
    if (user_dst) {
        return copyout(p->pagetable, dst, src, len);
    } else {
        memmove((char *)dst, src, len);
        return 0;
    }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int either_copyin(void *dst, int user_src, uint64 src, uint64 len) {
    struct proc *p = myproc();
    if (user_src) {
        return copyin(p->pagetable, dst, src, len);
    } else {
        memmove(dst, (char *)src, len);
        return 0;
    }
}