#ifndef __FMUTEX_H__
#define __FMUTEX_H__
#include "atomic/spinlock.h"
#include "lib/queue.h"
#include "common.h"

#define FUTEX_PRIVATE_FLAG 128
// 1000 0000
#define FUTEX_CLOCK_REALTIME 256
// 0001 0000 0000
#define FUTEX_CMD_MASK ~(FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME)
#define FUTEX_WAIT 0
#define FUTEX_WAKE 1
#define FUTEX_REQUEUE 3

#define FUTEX_LOCK_PI 6
#define FUTEX_WAIT_BITSET 9
#define FUTEX_WAIT_REQUEUE_PI 11
#define FUTEX_WAIT_REQUEUE_PI_PRIVATE (FUTEX_WAIT_REQUEUE_PI | FUTEX_PRIVATE_FLAG)

#define FUTEX_CMP_REQUEUE 4
#define FUTEX_CMP_REQUEUE_PI 12
#define FUTEX_WAKE_OP 5

#define FLAGS_SHARED 0x00
#define FLAGS_CLOCKRT 0x02
#define FUTEX_BITSET_MATCH_ANY 0xffffffff

struct futex {
    struct spinlock lock;
    struct Queue waiting_queue;
};

struct futex *get_futex(uint64 uaddr, int assert);
void futex_init(struct futex *fp, char *name);
int futex_wait(uint64 uaddr, uint val, struct timespec *ts);
int futex_wakeup(uint64 uaddr, int nr_wake);
int futex_requeue(uint64 uaddr1, int nr_wake, uint64 uaddr2, int nr_requeue);

int do_futex(uint64 uaddr, int op, uint32 val, struct timespec *ts,
             uint64 uaddr2, uint32 val2, uint32 val3);

// struct futex_cond {
//     int value;
//     struct futex *fp;
// };

/**
 * futex_entry - get the struct for this entry
 * @ptr:	the &vlaue pointer.
 * @type:	the type of the futex_cond struct this is embedded in.
 * @member:	the name of the value within the struct.
 */

// #define futex_entry(ptr, type, member)
//     container_of(ptr, type, member)

// struct futex *getfutex(int *uaddr);

// void futex_wait(int *uaddr, int val, struct timespec* tsp);
// void futex_signal(int *uaddr);

// void futex_cond_wait(struct futex_cond *cond, struct spinlock *mutex);
// void futex_cond_signal(struct futex_cond *cond);

#endif