#include "common.h"
#include "memory/vma.h"
#include "riscv.h"
#include "kernel/trap.h"
#include "proc/pcb_life.h"
#include "memory/vm.h"
#include "memory/allocator.h"
#include "fs/vfs/fs.h"
#include "debug.h"
#include "memory/mm.h"

#define PAGEFAULT(format, ...) printf("[PAGEFAULT]: " format "\n", ##__VA_ARGS__);
#define CHECK_PERM(cause, vma) (((cause) == STORE_PAGEFAULT && (vma->perm & PERM_WRITE))  \
                                || ((cause) == LOAD_PAGEFAULT && (vma->perm & PERM_READ)) \
                                || ((cause) == INSTUCTION_PAGEFAULT && (vma->perm & PERM_EXEC)))

/* copy-on write */
int cow(pagetable_t pagetable, uint64 stval);

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
    /* the va exceed the MAXVA is illegal */
    if (PGROUNDDOWN(stval) >= MAXVA) {
        PAGEFAULT("exceed the MAXVA");
        return -1;
    }

    struct vma *vma = find_vma_for_va(proc_current()->mm, stval);
    if (vma != NULL) {
        if (vma->type == VMA_FILE) {
            if (CHECK_PERM(cause, vma)) {
                uvmalloc(pagetable, PGROUNDDOWN(stval), PGROUNDUP(stval + 1), perm_vma2pte(vma->perm));
                paddr_t pa = walkaddr(pagetable, stval);
                fat32_inode_lock(vma->fp->f_tp.f_inode);
                // fat32_inode_load_from_disk(vma->fp->f_tp.f_inode);

                fat32_inode_read(vma->fp->f_tp.f_inode, 0, pa, vma->offset + PGROUNDDOWN(stval) - vma->startva, PGSIZE);
                fat32_inode_unlock(vma->fp->f_tp.f_inode);
            } else {
                PAGEFAULT("permission checked failed");
            }
        } else if (vma->type == VMA_ANON) {
            // Log("hit");
            uvmalloc(pagetable, PGROUNDDOWN(stval), PGROUNDUP(stval + 1), perm_vma2pte(vma->perm));
        } else {
            return cow(pagetable, stval);
        }
        return 0;
    } else {
        PAGEFAULT("va is not in the vmas");
        return -1;
    }

    return 0;
}

int cow(pagetable_t pagetable, uint64 stval) {
    void *mem;
    pte_t *pte;
    uint64 pa;
    uint flags;
    int level;

    level = walk(pagetable, stval, 0, 0, &pte);
    /* try to write to va which is not in the proc's pagetable is illegal */
    if (pte == NULL) {
        PAGEFAULT("try to access va which is not in the pagetable");
        return -1;
    }

    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);

    /* guard page for stack */
    if ((flags & PTE_U) == 0) {
        PAGEFAULT("access guard page");
        return -1;
    }

    /* write to an unshared page is illegal */
    if ((flags & PTE_SHARE) == 0) {
        PAGEFAULT("try to write a readonly page");
        return -1;
    }

    /* write to readonly shared page is illegal */
    if ((flags & PTE_READONLY) > 0) {
        PAGEFAULT("try to write a readonly shared page");
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
        PAGEFAULT("the level of leaf pte is wrong");
        return -1;
    }

    *pte = PA2PTE((uint64)mem) | flags | PTE_W;
    kfree((void *)pa);
    return 0;
}