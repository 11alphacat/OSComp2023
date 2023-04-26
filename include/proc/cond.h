#ifndef __COND_H__
#define __COND_H__

#include "proc/wait_queue.h"

struct Waiting_Q;
// condition variable
struct cond {
    Waiting_Q_t waiting_queue;
};

void cond_init(struct cond *cond, char *name);

void cond_wait(struct cond *cond, struct spinlock *mutex);

void cond_signal(struct cond *cond);

void cond_broadcast(struct cond *cond);

void sleep(void *, struct spinlock *);
void wakeup(void *);

#endif