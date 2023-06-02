#ifndef __VMA_H__
#define __VMA_H__

#include "common.h"
#include "lib/list.h"

#define NVMA 1000

#include "common.h"
struct proc;

/* permission */
#define PERM_READ (1 << 0)  /* same as PROT_READ */
#define PERM_WRITE (1 << 1) /* same as PROT_WRITE */
#define PERM_EXEC (1 << 2)  /* same as PROT_EXEC */
#define PERM_SHARED (1 << 3)
// #define PERM_KERNEL_PGTABLE (1 << 6)

/* virtual memory area */
struct vma {
    enum {
        VMA_MAP_FILE,
        VMA_MAP_ANON, /* anonymous */
    } type;
    struct list_head node;
    vaddr_t startva;
    size_t size;
    uint32 perm;

    // temp
    int used;

    int fd;
    uint64 offset;
    struct file *fp;
};

extern struct vma vmas[NVMA];
int vma_map_file(struct proc *p, uint64 va, size_t len, uint64 perm, uint64 type,
                 int fd, off_t offset, struct file *fp);
int vma_map(struct proc *p, uint64 va, size_t len, uint64 perm, uint64 type);
int vmspace_unmap(struct proc *p, vaddr_t va, size_t len);

struct vma *find_vma_for_va(struct proc *p, vaddr_t addr);
vaddr_t find_mapping_space(vaddr_t start, size_t size);
int vmacopy(struct proc *newproc);
void vmafree(struct proc *p);

#endif // __VMA_H__