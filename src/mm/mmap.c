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

    print_vma(mm);
    return (void *)mapva;
}

// memory
/* int mprotect(void *addr, size_t len, int prot); */
uint64 sys_mprotect(void) {
    vaddr_t addr;
    size_t len;
    int prot;
    argaddr(0, &addr);
    argulong(1, &len);
    argint(2, &prot);
    // Log("%p", addr);

    struct vma *vma = find_vma_for_va(proc_current()->mm, addr);
    if (vma == NULL) {
        return -1;
    }
    if (vma->size != len || vma->startva != addr) {
        // return -1;
    }
    vma->perm = prot;
    return 0;
}