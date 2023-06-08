#include "common.h"
#include "proc/pipe.h"
#include "lib/riscv.h"
#include "param.h"
#include "atomic/spinlock.h"
#include "proc/pcb_life.h"

#include "kernel/trap.h"
#include "memory/allocator.h"
#include "atomic/cond.h"
#include "proc/signal.h"
// #include "fs/fat/fat32_mem.h"
// #include "fs/fat/fat32_file.h"
#include "fs/vfs/fs.h"
#include "fs/vfs/ops.h"
#include "lib/sbuf.h"
#include "debug.h"

int pipe_alloc(struct file **f0, struct file **f1) {
    struct pipe *pi = 0;
    *f0 = *f1 = 0;
    if ((*f0 = filealloc()) == 0 || (*f1 = filealloc()) == 0)
        goto bad;
    if ((pi = (struct pipe *)kalloc()) == 0)
        goto bad;

    sbuf_init(&pi->buffer, PIPESIZE);

    pi->readopen = 1;
    pi->writeopen = 1;

    (*f0)->f_type = FD_PIPE;
    (*f0)->f_flags = O_RDONLY;
    (*f0)->f_tp.f_pipe = pi;
    (*f1)->f_type = FD_PIPE;
    (*f1)->f_flags = O_WRONLY;
    (*f1)->f_tp.f_pipe = pi;
    return 0;
bad:
    if (pi)
        kfree((char *)pi);
    if (*f0)
        generic_fileclose(*f0);
    if (*f1)
        generic_fileclose(*f1);
    return -1;
}

void pipe_close(struct pipe *pi, int writable) {
    acquire(&pi->buffer.lock);
    if (writable) {
        pi->writeopen = 0;
        sema_signal(&pi->buffer.items); // !!! bug
    } else {
        pi->readopen = 0;
    }
    if (pi->readopen == 0 && pi->writeopen == 0) {
        release(&pi->buffer.lock);
        sbuf_free(&pi->buffer); // !!!
        kfree((char *)pi);
    } else {
        release(&pi->buffer.lock);
    }
}

int pipe_write(struct pipe *pi, int user_dst, uint64 addr, int n) {
    struct proc *pr = proc_current();
    int i;
    for (i = 0; i < n; i++) {
        if (proc_killed(pr) || pi->readopen == 0) {
            return -1;
        }
        int ret = sbuf_insert(&pi->buffer, user_dst, addr + i);
        if (ret == -1) {
            return -1;
        }
    }
    return i;
}

int pipe_read(struct pipe *pi, int user_dst, uint64 addr, int n) {
    int i;
    struct proc *pr = proc_current();

    for (i = 0; i < n; i++) {
        if (proc_killed(pr)) {
            return -1;
        }
        if (pi->writeopen == 0 && pi->buffer.r == pi->buffer.w) { // !!! bug
            break;
        }
        int ret = sbuf_remove(&pi->buffer, user_dst, addr + i);
        if (ret == -1) {
            return -1;
        } else if (ret == 1) {
            break;
        }
    }
    return i;
}