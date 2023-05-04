#include "common.h"
#include "fs/inode/fs.h"
#include "fs/inode/file.h"
#include "memory/vma.h"
#include "riscv.h"
#include "kernel/trap.h"
#include "kernel/proc.h"
#include "memory/vm.h"
#include "memory/alloactor.h"

static uint32 perm_vma2pte(uint32 vma_perm) {
    uint32 pte_perm = 0;
    if (vma_perm & PERM_READ) {
        pte_perm |= PTE_R;
    }
    if (vma_perm & PERM_WRITE) {
        pte_perm |= PTE_W;
    }
    if (vma_perm & PERM_EXEC) {
        pte_perm |= PTE_X;
    }
    return pte_perm;
}

int pagefault(uint64 cause, pagetable_t pagetable, vaddr_t stval) {
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

    struct vma *vma = find_vma_for_va(myproc(), stval);
    if (vma != NULL) {
        if (vma->type == VMA_MAP_FILE) {
            if ((cause == STORE_PAGEFAULT && (vma->perm & PERM_WRITE))
                || (cause == LOAD_PAGEFAULT && (vma->perm & PERM_READ))
                || (cause == INSTUCTION_PAGEFAULT && (vma->perm & PERM_EXEC))) {
                uvmalloc(pagetable, PGROUNDDOWN(stval), PGROUNDUP(stval + 1), perm_vma2pte(vma->perm));
                paddr_t pa = walkaddr(pagetable, stval);
                ilock(vma->fp->ip);
                readi(vma->fp->ip, 0, pa, vma->offset + PGROUNDDOWN(stval) - vma->startva, PGSIZE);
                iunlock(vma->fp->ip);
            }
        }
        return 0;
    }

    /* copy-on-write */

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