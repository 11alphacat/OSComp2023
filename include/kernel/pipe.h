#ifndef __PIPE_H__
#define __PIPE_H__

#include "common.h"
#include "atomic/spinlock.h"

struct _file;
#define PIPESIZE 512

struct pipe {
    struct spinlock lock;
    char data[PIPESIZE];
    uint nread;    // number of bytes read
    uint nwrite;   // number of bytes written
    int readopen;  // read fd is still open
    int writeopen; // write fd is still open
};

int pipealloc(struct _file **f0, struct _file **f1);
void pipeclose(struct pipe *pi, int writable);
int piperead(struct pipe *pi, uint64 addr, int n);
int pipewrite(struct pipe *pi, uint64 addr, int n);

#endif // __PIPE_H__