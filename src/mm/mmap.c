#include "common.h"
#include "kernel/trap.h"
#include "proc/pcb_life.h"
#include "debug.h"
#include "memory/vma.h"
#include "fs/fcntl.h"
#include "memory/vm.h"
#include "lib/riscv.h"
#include "fs/vfs/fs.h"

// return (void *)0xfffff...ff to indicate fail
#define MAP_FAILED ((void *)-1)

/* int munmap(void *addr, size_t length); */
uint64 sys_munmap(void) {
    vaddr_t addr;
    size_t length;
    argaddr(0, &addr);
    argulong(1, &length);

    if (vmspace_unmap(proc_current(), addr, length) != 0) {
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
        return MAP_FAILED;
    }
    arglong(5, &offset);

    if (addr != 0 || offset != 0) {
        Log("mmap: not support");
        return MAP_FAILED;
    }

    vaddr_t mapva = find_mapping_space(addr, length);
    if (vma_map_file(proc_current(), mapva, length, mkperm(prot, flags), VMA_MAP_FILE, fd, offset, fp) < 0) {
        return MAP_FAILED;
    }

    return (void *)mapva;
}