#include "common.h"
#include "kernel/trap.h"
#include "proc/pcb_life.h"
#include "debug.h"
#include "memory/vma.h"
#include "fs/fcntl.h"
#include "memory/vm.h"
#include "lib/riscv.h"
#include "fs/vfs/fs.h"
#include "kernel/syscall.h"

// mmap
#define MAP_FILE 0
#define MAP_SHARED 0x01
#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20 /* Don't use a file.  */

// return (void *)0xfffff...ff to indicate fail
#define MAP_FAILED ((void *)-1)

/* int munmap(void *addr, size_t length); */
uint64 sys_munmap(void) {
    vaddr_t addr;
    size_t length;
    argaddr(0, &addr);
    argulong(1, &length);

    if (vmspace_unmap(proc_current()->mm, addr, length) != 0) {
        return -1;
    }
    return 0;
}

static uint64 mkperm(int prot, int flags) {
    uint64 perm = 0;
    if (flags & MAP_SHARED) {
        perm |= PERM_SHARED;
    }
    return (perm | prot);
}

// return type!!!
// 与声明不兼容不会出错
/* void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset); */
void *sys_mmap(void) {
    vaddr_t addr;
    size_t length;
    int prot;
    int flags;
    int fd;
    off_t offset;
    struct file *fp;

    argaddr(0, &addr);
    argulong(1, &length);
    argint(2, &prot);
    argint(3, &flags);
    if (argfd(4, &fd, &fp) < 0) {
        if ((flags & MAP_ANONYMOUS) == 0) {
            return MAP_FAILED;
        }
    }
    arglong(5, &offset);

    if (addr != 0 || offset != 0) {
        Warn("mmap: not support");
        return MAP_FAILED;
    }

    struct mm_struct *mm = proc_current()->mm;
    vaddr_t mapva = find_mapping_space(mm, addr, length);
    if (flags & MAP_ANONYMOUS) {
        if (vma_map(mm, mapva, length, mkperm(prot, flags), VMA_ANON) < 0) {
            return MAP_FAILED;
        }
    } else {
        if (vma_map_file(mm, mapva, length, mkperm(prot, flags), VMA_FILE, fd, offset, fp) < 0) {
            return MAP_FAILED;
        }
    }

    // print_vma(&mm->head_vma);
    return (void *)mapva;
}

/* int mprotect(void *addr, size_t len, int prot); */
uint64 sys_mprotect(void) {
    vaddr_t start;
    size_t len;
    int prot;
    argaddr(0, &start);
    argulong(1, &len);
    argint(2, &prot);

    if (!len)
        return 0;
    len = PGROUNDUP(len);
    uint64 end = start + len;
    if (end < start)
        return -1;

    if (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC))
        return -1;

    struct vma *vma = find_vma_for_va(proc_current()->mm, start);
    if (vma == NULL) {
        return -1;
    }

    struct mm_struct *mm = proc_current()->mm;
    // print_vma(&mm->head_vma);
    if (start != vma->startva) {
        if (split_vma(mm, vma, start, 1) < 0) {
            return -1;
        }
    }

    if (end != vma->startva + vma->size) {
        if (split_vma(mm, vma, end, 0) < 0) {
            return -1;
        }
    }
    // printfYELLOW("==========================");
    // print_vma(&mm->head_vma);

    vma->perm = prot;
    return 0;
}