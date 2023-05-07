#include "common.h"
#include "proc/pipe.h"
#include "riscv.h"
#include "param.h"
#include "atomic/spinlock.h"
#include "proc/pcb_life.h"

#include "kernel/trap.h"
#include "memory/allocator.h"
#include "proc/cond.h"
#include "proc/signal.h"
#include "fs/fat/fat32_mem.h"
#include "fs/fat/fat32_file.h"
#include "fs/vfs/fs.h"

int _pipealloc(struct _file **f0, struct _file **f1) {
    struct pipe *pi;

    pi = 0;
    *f0 = *f1 = 0;
    if ((*f0 = fat32_filealloc()) == 0 || (*f1 = fat32_filealloc()) == 0)
        goto bad;
    if ((pi = (struct pipe *)kalloc()) == 0)
        goto bad;
    pi->readopen = 1;
    pi->writeopen = 1;
    pi->nwrite = 0;
    pi->nread = 0;
    initlock(&pi->lock, "pipe");

    sema_init(&pi->read_sem, 0, "read_sem");
    sema_init(&pi->write_sem, 0, "write_sem");

    (*f0)->f_type = FD_PIPE;
    // (*f0)->readable = 1;
    // (*f0)->writable = 0;
    (*f0)->f_mode = O_RDONLY;
    (*f0)->f_tp.f_pipe = pi;
    (*f1)->f_type = FD_PIPE;
    // (*f1)->readable = 0;
    // (*f1)->writable = 1;
    return 0;

bad:
    if (pi)
        kfree((char *)pi);
    if (*f0)
        fat32_fileclose(*f0);
    if (*f1)
        fat32_fileclose(*f1);
    return -1;
}

// int pipealloc(struct file **f0, struct file **f1) {
//     struct pipe *pi;

//     pi = 0;
//     *f0 = *f1 = 0;
//     if ((*f0 = filealloc()) == 0 || (*f1 = filealloc()) == 0)
//         goto bad;
//     if ((pi = (struct pipe *)kalloc()) == 0)
//         goto bad;
//     pi->readopen = 1;
//     pi->writeopen = 1;
//     pi->nwrite = 0;
//     pi->nread = 0;
//     initlock(&pi->lock, "pipe");
//     (*f0)->type = FD_PIPE;
//     (*f0)->readable = 1;
//     (*f0)->writable = 0;
//     (*f0)->pipe = pi;
//     (*f1)->type = FD_PIPE;
//     (*f1)->readable = 0;
//     (*f1)->writable = 1;
//     (*f1)->pipe = pi;
//     return 0;

// bad:
//     if (pi)
//         kfree((char *)pi);
//     if (*f0)
//         fat32_fileclose(*f0);
//     if (*f1)
//         fat32_fileclose(*f1);
//     return -1;
// }

void pipeclose(struct pipe *pi, int writable) {
    acquire(&pi->lock);
    if (writable) {
        pi->writeopen = 0;
        // wakeup(&pi->nread);
        sema_signal(&pi->read_sem);
    } else {
        pi->readopen = 0;
        // wakeup(&pi->nwrite);
        sema_signal(&pi->write_sem);
    }
    if (pi->readopen == 0 && pi->writeopen == 0) {
        release(&pi->lock);
        kfree((char *)pi);
    } else
        release(&pi->lock);
}

int pipewrite(struct pipe *pi, uint64 addr, int n) {
    int i = 0;
    struct proc *pr = current();

    acquire(&pi->lock);
    while (i < n) {
        if (pi->readopen == 0 || killed(pr)) {
            release(&pi->lock);
            return -1;
        }
        if (pi->nwrite == pi->nread + PIPESIZE) { // DOC: pipewrite-full
            sema_signal(&pi->read_sem);

            release(&pi->lock);
            sema_wait(&pi->write_sem);
            acquire(&pi->lock);
            // wakeup(&pi->nread);
            // sleep(&pi->nwrite, &pi->lock);
        } else {
            char ch;
            if (copyin(pr->pagetable, &ch, addr + i, 1) == -1)
                break;
            pi->data[pi->nwrite++ % PIPESIZE] = ch;
            i++;
        }
    }
    // wakeup(&pi->nread);
    sema_signal(&pi->read_sem);
    release(&pi->lock);

    return i;
}

int piperead(struct pipe *pi, uint64 addr, int n) {
    int i;
    struct proc *pr = current();
    char ch;

    acquire(&pi->lock);
    while (pi->nread == pi->nwrite && pi->writeopen) { // DOC: pipe-empty
        if (killed(pr)) {
            release(&pi->lock);
            return -1;
        }
        release(&pi->lock);
        sema_wait(&pi->read_sem);
        acquire(&pi->lock);
        // sleep(&pi->nread, &pi->lock); // DOC: piperead-sleep
    }
    for (i = 0; i < n; i++) { // DOC: piperead-copy
        if (pi->nread == pi->nwrite)
            break;
        ch = pi->data[pi->nread++ % PIPESIZE];
        if (copyout(pr->pagetable, addr + i, &ch, 1) == -1)
            break;
    }
    // wakeup(&pi->nwrite); // DOC: piperead-wakeup
    sema_signal(&pi->write_sem);
    release(&pi->lock);
    return i;
}
