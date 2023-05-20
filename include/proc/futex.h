#ifndef __FMUTEX_H__
#define __FMUTEX_H__
#include "common.h"
#include "proc/wait_queue.h"
#include "atomic/spinlock.h"

struct futex {
    struct Waiting_Q waiting_queue;
    struct spinlock lock;
};

struct futex_cond {
    int value;
    struct futex *fp;
};

/**
 * futex_entry - get the struct for this entry
 * @ptr:	the &vlaue pointer.
 * @type:	the type of the futex_cond struct this is embedded in.
 * @member:	the name of the value within the struct.
 */
#define futex_entry(ptr, type, member) \
    container_of(ptr, type, member)

struct futex *getfutex(int *uaddr);

void futex_wait(int *uaddr, int val);
void futex_signal(int *uaddr);

void futex_cond_wait(struct futex_cond *cond, struct spinlock *mutex);
void futex_cond_signal(struct futex_cond *cond);

#endif