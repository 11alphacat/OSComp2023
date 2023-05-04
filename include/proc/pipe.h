#ifndef __PIPE_H__
#define __PIPE_H__

#include "common.h"
#include "atomic/spinlock.h"
#include "atomic/semaphore.h"

struct file;
#define PIPESIZE 512

struct pipe {
    struct spinlock lock;
    char data[PIPESIZE];
    uint nread;    // number of bytes read
    uint nwrite;   // number of bytes written
    int readopen;  // read fd is still open
    int writeopen; // write fd is still open

    struct semaphore read_sem;
    struct semaphore write_sem;
};

int pipealloc(struct file **, struct file **);
void pipeclose(struct pipe *, int);
int piperead(struct pipe *, uint64, int);
int pipewrite(struct pipe *, uint64, int);

#endif // __PIPE_H__