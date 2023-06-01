#include "atomic/spinlock.h"
#include "common.h"
#include "proc/tcb_queue.h"
#include "proc/wait_queue.h"
#include "proc/sched.h"
#include "atomic/futex.h"
#include "list.h"
#include "debug.h"

// global futex struct table (similar to inode table)
struct futex futex_table[FUTEX_NUM];

void futex_table_init() {
    struct futex *entry;
    for (entry = futex_table; entry < &futex_table[FUTEX_NUM]; entry++) {
        initlock(&entry->lock, "futex lock");
        Waiting_Q_init(&entry->waiting_queue, "futex queue lock");
        entry->uaddr = 0;
        entry->valid = 0;
    }
}

struct futex *futex_init(uint64 uaddr) {
    struct futex *entry;
    for (entry = futex_table; entry < &futex_table[FUTEX_NUM]; entry++) {
        if (!entry->valid) {
            entry->valid = 1;
            entry->uaddr = uaddr;
            return entry;
        }
    }
    return NULL;
}

struct futex *futex_get(uint64 uaddr) {
    struct futex *entry;
    for (entry = futex_table; entry < &futex_table[FUTEX_NUM]; entry++) {
        if (entry->valid && entry->uaddr == uaddr) {
            return entry;
        }
    }
    return NULL;
}

void futex_clear(uint64 uaddr) {
    struct futex *entry;
    for (entry = futex_table; entry < &futex_table[FUTEX_NUM]; entry++) {
        if (entry->valid && entry->uaddr == uaddr) {
            entry->valid = 0;
            entry->uaddr = 0;
            return;
        }
    }
}

int futex_wait(uint64 uaddr, uint val, struct timespec ts) {
    struct futex *fp;
    if ((fp = futex_init(uaddr)) == NULL) {
        printfRed("no emptry entry for futex table\n");
        return -1;
    }
    struct tcb *t = thread_current();
    if (t) {
        do_sleep_ns(t, ts);
    }

    return 0;
}

// struct futex *getfutex(int *uaddr) {
//     struct futex_cond *p = futex_entry(uaddr, struct futex_cond, value);
//     return p->fp;
// };

// void futex_wait(uint64 uaddr, struct timespec* tsp) {
//     struct futex *fp = getfutex(uaddr);
//     acquire(&fp->lock);
//     if (*uaddr == val) {
//         struct tcb *t = thread_current();

//         acquire(&t->lock);
//         TCB_Q_changeState(t, TCB_SLEEPING);
//         Waiting_Q_push_back(&fp->waiting_queue, t);
//         release(&fp->lock);

//         thread_sched();
//         release(&t->lock);
//     } else {
//         release(&fp->lock);
//     }
// }

// void futex_wake(uint64 uaddr) {
//     struct futex *fp = getfutex(uaddr);
//     struct tcb *t;

//     acquire(&fp->lock);

//     if (!Waiting_Q_isempty_atomic(&fp->waiting_queue)) {
//         t = Waiting_Q_provide(&fp->waiting_queue);
//         if (t == NULL)
//             panic("cond signal : this cond has no object waiting queue");
//         if (t->state != TCB_SLEEPING) {
//             printf("%s\n", t->state);
//             panic("cond signal : this thread is not sleeping");
//         }
//         acquire(&t->lock);
//         TCB_Q_changeState(t, TCB_RUNNABLE);
//         release(&t->lock);
//     }

//     release(&fp->lock);
// }

// void futex_cond_wait(struct futex_cond *cond, struct spinlock *mutex) {
//     return ;
//     // uint local = cond->value;
//     // release(mutex);
//     // futex_wait(&cond->value, local);
//     // acquire(mutex);
// }

// void futex_cond_signal(struct futex_cond *cond) {
//     cond->value += 1;
//     futex_signal(&cond->value);
// }

// #define futex_cmpxchg_enabled 1

int do_futex(uint64 uaddr, int op, uint32 val, struct timespec ts,
             uint64 uaddr2, uint32 val2, uint32 val3) {
    int cmd = op & FUTEX_CMD_MASK;
    uint flags = 0;

    if (!(op & FUTEX_PRIVATE_FLAG))
        flags |= FLAGS_SHARED;

    if (op & FUTEX_CLOCK_REALTIME) {
        flags |= FLAGS_CLOCKRT;
        if (cmd != FUTEX_WAIT && cmd != FUTEX_WAIT_BITSET && cmd != FUTEX_WAIT_REQUEUE_PI)
            return -1;
    }
    // switch (cmd) {
    // case FUTEX_LOCK_PI:
    // case FUTEX_UNLOCK_PI:
    // case FUTEX_TRYLOCK_PI:
    // case FUTEX_WAIT_REQUEUE_PI:
    // case FUTEX_CMP_REQUEUE_PI:
    // 	if (!futex_cmpxchg_enabled)
    // 		return -ENOSYS;
    // }
    // switch (cmd) {
    //     case FUTEX_WAIT:
    //         val3 = FUTEX_BITSET_MATCH_ANY;
    //     case FUTEX_WAIT_BITSET:
    // 		return do_futex_wait(uaddr, flags, val, timeout, val3);
    //     case FUTEX_WAKE:
    // 	    val3 = FUTEX_BITSET_MATCH_ANY;
    // }

    return 0;
}