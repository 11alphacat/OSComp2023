#ifndef __SBUF_H__
#define __SBUF_H__

#include "atomic/spinlock.h"
#include "atomic/semaphore.h"

enum sbuf_type {
    PIPE_SBUF,
    CONSOLE_SBUF
};

struct sbuf {
    enum sbuf_type type;
    struct spinlock lock;
    char *buf;
    uint n;
    uint w;
    uint r;
    struct semaphore slots;
    struct semaphore items;
};

void sbuf_init(struct sbuf *buffer, uint n);
void sbuf_free(struct sbuf *buffer);
int sbuf_insert(struct sbuf *sp, int user_dst, uint64 addr);
int sbuf_remove(struct sbuf *sp, int user_dst, uint64 addr);

#endif