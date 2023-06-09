#ifndef __PIPE_H__
#define __PIPE_H__

#include "common.h"
#include "atomic/spinlock.h"
#include "atomic/semaphore.h"
#include "lib/sbuf.h"

struct file;

#define PIPESIZE 512

struct pipe {
    struct sbuf buffer;
    int readopen;  // read fd is still open
    int writeopen; // write fd is still open
};

int pipe_alloc(struct file **f0, struct file **f1);
void pipe_close(struct pipe *pi, int writable);
int pipe_write(struct pipe *pi, int user_dst, uint64 addr, int n);
int pipe_read(struct pipe *pi, int user_dst, uint64 addr, int n);

#endif // __PIPE_H__