#include "param.h"
#include "common.h"
#include "memory/memlayout.h"
#include "proc/exec.h"
#include "riscv.h"
#include "fs/inode/fs.h"
#include "memory/vm.h"
#include "debug.h"
#include "memory/allocator.h"
#include "proc/pcb_life.h"
#include "proc/pcb_mm.h"

/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[]; // kernel.ld sets this to end of kernel code.

extern char trampoline[]; // trampoline.S

// Make a direct-map page table for the kernel.
pagetable_t
kvmmake(void) {
    pagetable_t kpgtbl;

    kpgtbl = (pagetable_t)kzalloc(PGSIZE);

    // uart registers
    kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

    // virtio mmio disk interface
    kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

    // PLIC
    kvmmap(kpgtbl, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

    // CLINT_MTIME
    // map in kernel pagetable, so we can access it in s-mode
    kvmmap(kpgtbl, CLINT_MTIME, CLINT_MTIME, PGSIZE, PTE_R);

    // map kernel text executable and read-only.
    kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext - KERNBASE, PTE_R | PTE_X);

    // map kernel data and the physical RAM we'll make use of.
    kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP - (uint64)etext, PTE_R | PTE_W);

    // map the trampoline for trap entry/exit to
    // the highest virtual address in the kernel.
    kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

    // allocate and map a kernel stack for each process.
    proc_mapstacks(kpgtbl);

    return kpgtbl;
}

// Initialize the one kernel_pagetable
void kvminit(void) {
    kernel_pagetable = kvmmake();
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
void kvminithart() {
    // wait for any previous writes to the page table memory to finish.
    sfence_vma();

    w_satp(MAKE_SATP(kernel_pagetable));

    // flush stale entries from the TLB.
    sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
#define LEVELS 3
/* return the leaf pte's level, set (pte_t *)*pte to this pte's address at the same time */
int walk(pagetable_t pagetable, uint64 va, int alloc, int lowlevel, pte_t **pte) {
    if (va >= MAXVA)
        panic("walk");

    for (int level = LEVELS - 1; level > lowlevel; level--) {
        pte_t *pte_tmp = &pagetable[PN(level, va)];
        if (*pte_tmp & PTE_V) {
            // find a superpage leaf PTE
            if ((*pte_tmp & PTE_R) || (*pte_tmp & PTE_X)) {
                *pte = pte_tmp;
                /* assert: only support 2MB leaf-pte */
                ASSERT(level == 1);
                return level;
            }
            pagetable = (pagetable_t)PTE2PA(*pte_tmp);
        } else {
            if (!alloc || (pagetable = (pde_t *)kzalloc(PGSIZE)) == 0) {
                *pte = 0;
                return -1;
            }
            *pte_tmp = PA2PTE(pagetable) | PTE_V;
        }
    }
    *pte = (pte_t *)&pagetable[PN(lowlevel, va)];
    return 0;
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64 walkaddr(pagetable_t pagetable, uint64 va) {
    pte_t *pte;
    uint64 pa;

    if (va >= MAXVA)
        return 0;

    int level = walk(pagetable, va, 0, 0, &pte);
    ASSERT(level <= 1);
    if (pte == 0)
        return 0;
    if ((*pte & PTE_V) == 0)
        return 0;
    if ((*pte & PTE_U) == 0)
        return 0;
    pa = PTE2PA(*pte);
    if (level == COMMONPAGE) {
        return pa;
    } else if (level == SUPERPAGE) {
        return pa + (PGROUNDDOWN(va) - SUPERPG_DOWN(va));
    } else {
        panic("can not reach here");
    }
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm) {
    if (mappages(kpgtbl, va, sz, pa, perm, 0) != 0)
        panic("kvmmap");
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm, int lowlevel) {
    uint64 a, last;
    pte_t *pte;

    if (size == 0)
        panic("mappages: size");

    uint64 pgsize = 0;
    switch (lowlevel) {
    case 0: {
        pgsize = PGSIZE;
        a = PGROUNDDOWN(va);
        last = PGROUNDDOWN(va + size - 1);
        break;
    }
    case 1: {
        pgsize = SUPERPGSIZE;
        /* if this is a superpage mapping, the va needs to be superpage aligned */
        ASSERT(va % SUPERPGSIZE == 0);
        a = SUPERPG_DOWN(va);
        last = SUPERPG_DOWN(va + size - 1);
        break;
    }
    default: panic("mappages: not support"); break;
    }

    for (;;) {
        walk(pagetable, a, 1, lowlevel, &pte);
        if (pte == 0) {
            return -1;
        }
        if (*pte & PTE_V) {
            vmprint(pagetable, 1, 0, 0, 0);
            Log("remap pte is %x", *pte);
            Log("remap pa is %x", PTE2PA(*pte));
            panic("mappages: remap");
        }
        *pte = PA2PTE(pa) | perm | PTE_V;
        if (a == last)
            break;
        a += pgsize;
        pa += pgsize;
    }
    return 0;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
/* on_demand option: use for on-demand mapping, only unmap the mapping pages 
                     and skip the unmapping pages */
void uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free, int on_demand) {
    uint64 a;
    pte_t *pte;

    if ((va % PGSIZE) != 0)
        panic("uvmunmap: not aligned");

    uint64 endva = va + npages * PGSIZE;

    for (a = va; a < endva; a += PGSIZE) {
        int level = walk(pagetable, a, 0, 0, &pte);
        if (pte == 0) {
            if (on_demand == 1) {
                continue;
            }
            panic("uvmunmap: walk");
        }
        if ((*pte & PTE_V) == 0) {
            if (on_demand == 1) {
                continue;
            }
            vmprint(pagetable, 1, 0, 0, 0);
            printf("va is %x\n", va);
            panic("uvmunmap: not mapped");
        }
        if (PTE_FLAGS(*pte) == PTE_V)
            panic("uvmunmap: not a leaf");

        ASSERT(level <= 1);

        if (do_free) {
            uint64 pa = PTE2PA(*pte);
            kfree((void *)pa);
        }
        uint64 pte_flags = PTE_FLAGS(*pte);
        *pte = 0;

        if (level == SUPERPAGE) {
            if (a != SUPERPG_DOWN(a) && a == va) {
                uvmalloc(pagetable, SUPERPG_DOWN(a), a, pte_flags);
                // vmprint(pagetable, 1, 0, 0, 0);
            }
            a += (SUPERPGSIZE - PGSIZE);
        }
    }
}

// create an empty user page table.
// returns 0 if out of memory.
pagetable_t uvmcreate() {
    pagetable_t pagetable;
    pagetable = (pagetable_t)kalloc();
    if (pagetable == 0)
        return 0;
    memset(pagetable, 0, PGSIZE);
    return pagetable;
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64 uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm) {
    char *mem;

    if (newsz < oldsz)
        return oldsz;

    // 外层有对super/normal page 的判断!

    uint64 aligned_sz = PGROUNDUP(oldsz);
    uint64 super_aligned_sz = SUPERPG_ROUNDUP(oldsz);
    uint64 min = (newsz <= super_aligned_sz ? newsz : super_aligned_sz);

    for (uint64 a = aligned_sz; a < min; a += PGSIZE) {
        mem = kzalloc(PGSIZE);
        if (mem == 0) {
            uvmdealloc(pagetable, a, oldsz);
            return 0;
        }
        if (mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_R | PTE_U | xperm, COMMONPAGE) != 0) {
            kfree(mem);
            uvmdealloc(pagetable, a, oldsz);
            return 0;
        }
    }

    if (newsz <= super_aligned_sz) {
        ASSERT(min == newsz);
        return newsz;
    } else {
        uint64 addr;
        uint64 newsz_down = SUPERPG_DOWN(newsz);
        for (addr = super_aligned_sz; addr < newsz_down; addr += SUPERPGSIZE) {
            mem = kzalloc(SUPERPGSIZE);
            if (mem == 0) {
                // vmprint(pagetable, 1, 0, 0, 0);
                uvmdealloc(pagetable, addr, oldsz);
                return 0;
            }
            if (mappages(pagetable, addr, SUPERPGSIZE, (uint64)mem, PTE_R | PTE_U | xperm, SUPERPAGE) != 0) {
                kfree(mem);
                uvmdealloc(pagetable, addr, oldsz);
                return 0;
            }
        }

        for (addr = newsz_down; addr < newsz; addr += PGSIZE) {
            mem = kzalloc(PGSIZE);
            if (mem == 0) {
                uvmdealloc(pagetable, addr, oldsz);
                return 0;
            }
            if (mappages(pagetable, addr, PGSIZE, (uint64)mem, PTE_R | PTE_U | xperm, COMMONPAGE) != 0) {
                kfree(mem);
                uvmdealloc(pagetable, addr, oldsz);
                return 0;
            }
        }
    }

    return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64 uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz) {
    if (newsz >= oldsz)
        return oldsz;

    if (PGROUNDUP(newsz) < PGROUNDUP(oldsz)) {
        int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
        uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1, 0);
    }

    return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void freewalk(pagetable_t pagetable) {
    // there are 2^9 = 512 PTEs in a page table.
    for (int i = 0; i < 512; i++) {
        pte_t pte = pagetable[i];
        if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0) {
            // this PTE points to a lower-level page table.
            uint64 child = PTE2PA(pte);
            freewalk((pagetable_t)child);
            pagetable[i] = 0;
        } else if (pte & PTE_V) {
            panic("freewalk: leaf");
        }
    }
    kfree((void *)pagetable);
}

// Free user memory pages,
// then free page-table pages.
void uvmfree(pagetable_t pagetable, uint64 sz) {
    if (sz > 0)
        uvmunmap(pagetable, 0, PGROUNDUP(sz) / PGSIZE, 1, 0);
    freewalk(pagetable);
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz) {
    pte_t *pte;
    uint64 pa, i;
    uint flags;

    for (i = 0; i < sz; i += PGSIZE) {
        int level = walk(old, i, 0, 0, &pte);
        ASSERT(level <= 1 && level >= 0);
        if (pte == NULL) {
            panic("uvmcopy: pte should exist");
        }
        if ((*pte & PTE_W) == 0 && (*pte & PTE_SHARE) == 0) {
            *pte = *pte | PTE_READONLY;
        }
        /* shared page */
        if ((*pte & PTE_W) == 0 && (*pte & PTE_READONLY) != 0) {
            *pte = *pte | PTE_READONLY;
        }
        *pte = *pte | PTE_SHARE;
        *pte = *pte & ~PTE_W;
        pa = PTE2PA(*pte);
        flags = PTE_FLAGS(*pte);
        if (level == SUPERPAGE) {
            /* level == 1 ~ map superpage */
            ASSERT(i == SUPERPG_DOWN(i));
            if (mappages(new, i, SUPERPGSIZE, pa, flags, SUPERPAGE) != 0) {
                goto err;
            }
            i += (SUPERPGSIZE - PGSIZE);
        } else if (level == COMMONPAGE) {
            /* level == 0 ~ map common page */
            if (mappages(new, i, PGSIZE, pa, flags, COMMONPAGE) != 0) {
                goto err;
            }
        }
        /* call share_page after mappages success! */
        share_page(pa);
    }
    return 0;

err:
    uvmunmap(new, 0, i / PGSIZE, 0, 0);
    return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void uvmclear(pagetable_t pagetable, uint64 va) {
    pte_t *pte;

    walk(pagetable, va, 0, 0, &pte);
    if (pte == 0)
        panic("uvmclear");
    *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len) {
    uint64 n, va0, pa0;

    while (len > 0) {
        va0 = PGROUNDDOWN(dstva);

        // kernel has the right to write all RAM without PAGE FAULT
        // so need to check if this write is legal(PTE_W == 1 in PTE)
        // if not, call cow
        pte_t *pte;
        int flags;
        /* since walk will panic if va0 > maxva, so we have to handle this error before walk */
        if (va0 >= MAXVA) {
            return -1;
        }
        walk(pagetable, va0, 0, 0, &pte);
        if (pte == 0) {
            return -1;
        }
        flags = PTE_FLAGS(*pte);
        if ((flags & PTE_W) == 0) {
            int ret = cow(pagetable, va0);
            if (ret < 0) {
                return -1;
            }
        }

        pa0 = walkaddr(pagetable, va0);
        if (pa0 == 0)
            return -1;
        n = PGSIZE - (dstva - va0);
        if (n > len)
            n = len;
        memmove((void *)(pa0 + (dstva - va0)), src, n);

        len -= n;
        src += n;
        dstva = va0 + PGSIZE;
    }
    return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len) {
    uint64 n, va0, pa0;

    while (len > 0) {
        va0 = PGROUNDDOWN(srcva);
        pa0 = walkaddr(pagetable, va0);
        if (pa0 == 0)
            return -1;
        n = PGSIZE - (srcva - va0);
        if (n > len)
            n = len;
        memmove(dst, (void *)(pa0 + (srcva - va0)), n);

        len -= n;
        dst += n;
        srcva = va0 + PGSIZE;
    }
    return 0;
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max) {
    uint64 n, va0, pa0;
    int got_null = 0;

    while (got_null == 0 && max > 0) {
        va0 = PGROUNDDOWN(srcva);
        pa0 = walkaddr(pagetable, va0);
        if (pa0 == 0)
            return -1;
        n = PGSIZE - (srcva - va0);
        if (n > max)
            n = max;

        char *p = (char *)(pa0 + (srcva - va0));
        while (n > 0) {
            if (*p == '\0') {
                *dst = '\0';
                got_null = 1;
                break;
            } else {
                *dst = *p;
            }
            --n;
            --max;
            p++;
            dst++;
        }

        srcva = va0 + PGSIZE;
    }
    if (got_null) {
        return 0;
    } else {
        return -1;
    }
}

/* vpn ~ virtual page number */
void vmprint_indent(int level, int vpn) {
    switch (level) {
    case 0: printf(" ..%d: ", vpn); break;
    case 1: printf(" .. ..%d: ", vpn); break;
    case 2: printf(" .. .. ..%d: ", vpn); break;
    default: panic("should not reach here");
    }
}

void vmprint(pagetable_t pagetable, int isroot, int level, int single, uint64 vabase) {
    pte_t pte;
    if (isroot) {
        printf("page table %p\n", pagetable);
    }

    uint64 vagap;
    switch (level) {
    case 0: vagap = SUPERPGSIZE * 512; break;
    case 1: vagap = SUPERPGSIZE; break;
    case 2: vagap = PGSIZE; break;
    default: panic("wrong argument");
    }

    for (int i = 0; i < 512; i++) {
        pte = pagetable[i];
        if (pte & PTE_V) {
            vmprint_indent(level, i);
            if (!single && (pte & (PTE_W | PTE_X | PTE_R)) == 0) {
                // not a leaf-pte
                printf("pte %p pa %p\n", pte, PTE2PA(pte));
                vmprint((pagetable_t)PTE2PA(pte), 0, level + 1, 0, vabase + i * vagap);
            } else {
                // a leaf-pte
                printf("leaf pte %p pa %p ", pte, PTE2PA(pte));
                PTE("RSW %d%d U %d X %d W %d R %d  va is %x\n",
                    (pte & PTE_READONLY) > 0, (pte & PTE_SHARE) > 0,
                    (pte & PTE_U) > 0, (pte & PTE_X) > 0,
                    (pte & PTE_W) > 0, (pte & PTE_R) > 0,
                    vabase + i * vagap);
            }
        }
    }
}

int cow(pagetable_t pagetable, uint64 stval) {
    void *mem;
    pte_t *pte;
    uint64 pa;
    uint flags;
    int level;

    /* the va exceed the MAXVA is illegal */
    if (PGROUNDDOWN(stval) >= MAXVA) {
        printf("exceed the MAXVA");
        return -1;
    }

    level = walk(pagetable, stval, 0, 0, &pte);
    /* try to write to va which is not in the proc's pagetable is illegal */
    if (pte == NULL) {
        return -1;
    }
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);

    /* guard page for stack */
    if ((flags & PTE_U) == 0) {
        return -1;
    }
    /* write to an unshared page is illegal */
    if ((flags & PTE_SHARE) == 0) {
        printf("try to write a readonly page");
        return -1;
    }

    /* write to readonly shared page is illegal */
    if ((flags & PTE_READONLY) > 0) {
        printf("try to write a readonly page");
        return -1;
    }

    if (level == SUPERPAGE) {
        // 2MB superpage
        if ((mem = kmalloc(SUPERPGSIZE)) == 0) {
            return -1;
        }
        memmove(mem, (void *)pa, SUPERPGSIZE);
    } else if (level == COMMONPAGE) {
        // common page
        if ((mem = kmalloc(PGSIZE)) == 0) {
            return -1;
        }
        memmove(mem, (void *)pa, PGSIZE);
    } else {
        return -1;
    }

    *pte = PA2PTE((uint64)mem) | flags | PTE_W;
    kfree((void *)pa);
    return 0;
}