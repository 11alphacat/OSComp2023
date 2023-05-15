#include "common.h"
#include "memory/vma.h"
#include "atomic/spinlock.h"
#include "memory/allocator.h"
#include "proc/pcb_life.h"
#include "riscv.h"
#include "list.h"
#include "debug.h"
#include "memory/vm.h"

#include "fs/fat/fat32_file.h"
#include "fs/vfs/fs.h"
#include "fs/vfs/ops.h"

struct vma vmas[NVMA];
struct spinlock vmas_lock;

static struct vma *vma_map_range(struct proc *p, uint64 va, size_t len, uint64 perm, uint64 type);
void vmas_init() {
    initlock(&vmas_lock, "vmas_lock");
    memset(vmas, 0, sizeof(vmas));
}

static struct vma *alloc_vma(void) {
    // 1. slab allocator
    // 2. fine grained lock
    acquire(&vmas_lock);
    for (int i = 0; i < NVMA; i++) {
        if (vmas[i].used == 0) {
            vmas[i].used = 1;
            release(&vmas_lock);
            return &vmas[i];
        }
    }
    release(&vmas_lock);
    return 0;
}

void free_vma(struct vma *vma) {
    acquire(&vmas_lock);
    vma->used = 0;
    release(&vmas_lock);
}

/*
 * Returns 0 when no intersection detected.
 */
static int check_vma_intersect(struct list_head *vma_head, struct vma *checked_vma) {
    vaddr_t checked_start, start;
    vaddr_t checked_end, end;
    struct vma *pos;

    checked_start = checked_vma->startva;
    checked_end = checked_start + checked_vma->size - 1;

    list_for_each_entry(pos, vma_head, node) {
        start = pos->startva;
        end = pos->startva + pos->size;
        if ((checked_start >= start && checked_start < end) || (checked_end >= start && checked_end < end)) {
            return 1;
        }
    }
    return 0;
}

static int add_vma_to_vmspace(struct list_head *head, struct vma *vma) {
    if (check_vma_intersect(head, vma) != 0) {
        Log("add_vma_to_vmspace: vma overlap\n");
        ASSERT(0);
        return -1;
    }

    list_add(&vma->node, head);
    return 0;
}

static int is_vma_in_vmspace(struct list_head *vma_head, struct vma *vma) {
    struct vma *pos;

    list_for_each_entry(pos, vma_head, node) {
        if (pos == vma) {
            return 1;
        }
    }

    return 0;
}

static void del_vma_from_vmspace(struct list_head *vma_head, struct vma *vma) {
    if (is_vma_in_vmspace(vma_head, vma)) {
        list_del(&(vma->node));
    }
    free_vma(vma);
}

int vma_map_file(struct proc *p, uint64 va, size_t len, uint64 perm, uint64 type,
                 int fd, off_t offset, struct file *fp) {
    struct vma *vma;
    /* the file isn't writable and perm has PERM_WRITE is illegal
       but if the PERM_SHARED is not set(means PERM_PRIVATE), then it's ok */
    if (!F_WRITEABLE(fp) && ((perm & PERM_WRITE) && (perm & PERM_SHARED))) {
        return -1;
    }
    if ((vma = vma_map_range(p, va, len, perm, type)) == NULL) {
        return -1;
    }
    vma->fd = fd;
    vma->offset = offset;
    vma->fp = fp;
    fat32_filedup(vma->fp);
    return 0;
}

int vma_map(struct proc *p, uint64 va, size_t len, uint64 perm, uint64 type) {
    if (vma_map_range(p, va, len, perm, type) == NULL) {
        return -1;
    } else {
        return 0;
    }
}

static struct vma *vma_map_range(struct proc *p, uint64 va, size_t len, uint64 perm, uint64 type) {
    struct vma *vma;
    vma = alloc_vma();
    if (vma == NULL) {
        return 0;
    }

    // not support
    ASSERT(va % PGSIZE == 0);

    vma->startva = PGROUNDDOWN(va);
    if (len < PGSIZE) {
        len = PGSIZE;
    }
    vma->size = PGROUNDUP(len);
    vma->perm = perm;
    vma->type = type;

    if (add_vma_to_vmspace(&p->head_vma, vma) < 0) {
        goto free_vma;
    }
    return vma;

free_vma:
    free_vma(vma);
    return 0;
}

static void writeback(pagetable_t pagetable, struct file *fp, vaddr_t start, size_t len) {
    ASSERT(start % PGSIZE == 0);
    ASSERT(fp != NULL);

    pte_t *pte;
    vaddr_t endva = start + len;
    for (vaddr_t addr = start; addr < endva; addr += PGSIZE) {
        walk(pagetable, addr, 0, 0, &pte);
        if (pte == NULL || (*pte & PTE_V) == 0) {
            continue;
        }
        /* only writeback dirty pages(pages with PTE_D) */
        if (PTE_FLAGS(*pte) & PTE_D) {
            fat32_filewrite(fp, addr, (PGSIZE > (endva - addr) ? (endva - addr) : PGSIZE));
        }
    }
}

int vmspace_unmap(struct proc *p, vaddr_t va, size_t len) {
    struct vma *vma;
    vaddr_t start;
    size_t size;

    vma = find_vma_for_va(p, va);
    if (!vma) {
        return -1;
    }

    start = vma->startva;
    size = vma->size;

    if ((va != start)) {
        Log("we only support unmap at the start now.\n");
        return -1;
    }

    // ASSERT(len % PGSIZE == 0);
    size_t origin_len = len;
    len = PGROUNDUP(len);
    ASSERT(size >= len);
    // len = PGROUNDUP(len);

    if (vma->type == VMA_MAP_FILE) {
        /* if the perm has PERM_SHREAD, call writeback */
        if ((vma->perm & PERM_SHARED) && (vma->perm & PERM_WRITE)) {
            writeback(p->pagetable, vma->fp, start, origin_len);
        }
    }

    if (size > len) {
        /* unmap part of the vma */
        vma->startva += len;
        vma->size -= len;
        uvmunmap(p->pagetable, start, PGROUNDUP(size) / PGSIZE, 1, 1);
        return 0;
    } else {
        if (vma->type == VMA_MAP_FILE) {
            generic_fileclose(vma->fp);
        }
    }

    del_vma_from_vmspace(&p->head_vma, vma);

    // Note: non-leaf pte still not recycle
    uvmunmap(p->pagetable, start, PGROUNDUP(size) / PGSIZE, 1, 1);

    return 0;
}

struct vma *find_vma_for_va(struct proc *p, vaddr_t addr) {
    struct vma *pos;
    vaddr_t start, end;
    list_for_each_entry(pos, &p->head_vma, node) {
        start = pos->startva;
        end = pos->startva + pos->size;
        if (addr >= start && addr < end) {
            return pos;
        }
    }
    return NULL;
}

#define MMAP_START 0x10000000
vaddr_t find_mapping_space(vaddr_t start, size_t size) {
    struct proc *p = current();
    struct vma *pos;
    vaddr_t max = MMAP_START;
    list_for_each_entry(pos, &p->head_vma, node) {
        vaddr_t endva = pos->startva + pos->size;
        if (max < endva) {
            max = endva;
        }
    }
    ASSERT(max % PGSIZE == 0);

    // assert code: make sure the max address is not in pagetable(not mapping)
    pte_t *pte;
    int ret;
    ret = walk(current()->pagetable, max, 0, 0, &pte);
    ASSERT(ret == -1 || *pte == 0);
    return max;
}

void sys_print_vma() {
    struct proc *p = current();
    struct vma *pos;
    VMA("%s vmas:\n", current()->name);
    list_for_each_entry(pos, &p->head_vma, node) {
        VMA("%x-%x %dKB\t", pos->startva, pos->startva + pos->size, pos->size / PGSIZE);
        if (pos->perm & PERM_READ) {
            VMA("r");
        } else {
            VMA("-");
        }
        if (pos->perm & PERM_WRITE) {
            VMA("w");
        } else {
            VMA("-");
        }
        if (pos->perm & PERM_EXEC) {
            VMA("x");
        } else {
            VMA("-");
        }
        if (pos->perm & PERM_SHARED) {
            VMA("s");
        } else {
            VMA("p");
        }
        VMA("\n");
    }
}

int vmacopy(struct proc *newproc) {
    struct proc *p = current();
    struct vma *pos;
    list_for_each_entry(pos, &p->head_vma, node) {
        if (pos->type == VMA_MAP_FILE) {
            // int vma_map_file(struct proc *p, uint64 va, size_t len, uint64 perm, uint64 type,
            //                  int fd, off_t offset, struct file *fp) {
            if (vma_map_file(newproc, pos->startva, pos->size, pos->perm, pos->type,
                             pos->fd, pos->offset, pos->fp)
                < 0) {
                return -1;
            }
        } else {
            panic("not support");
        }
    }
    return 0;
}

void vmafree(struct proc *p) {
    struct vma *pos;
    struct vma *pos2;
    list_for_each_entry_safe(pos, pos2, &p->head_vma, node) {
        if (vmspace_unmap(p, pos->startva, pos->size) < 0) {
            panic("vmafree: unmap failed");
        }
    }
}
