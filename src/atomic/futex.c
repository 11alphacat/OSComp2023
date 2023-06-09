#include "atomic/futex.h"
#include "atomic/spinlock.h"
#include "proc/sched.h"
#include "lib/list.h"
#include "lib/queue.h"
#include "common.h"
#include "debug.h"
#include "lib/hash.h"
#include "common.h"

extern struct hash_table futex_map;

void futex_init(struct futex *fp, char *name) {
    initlock(&fp->lock, name);
    Queue_init(&fp->waiting_queue, name, TCB_WAIT_QUEUE);
}

struct futex *get_futex(uint64 uaddr) {
    struct hash_node *node;
    if ((node = hash_lookup(&futex_map, (void *)&uaddr, NULL, 1)) != NULL) { // release it
        return (struct futex *)(node->value);
    } else {
        struct futex *fp = (struct futex *)kmalloc(sizeof(struct futex));
        if (fp == NULL) {
            panic("get_futex : no free space\n");
        }
        futex_init(fp, "futex");
        hash_insert(&futex_map, (void *)&uaddr, (void *)fp);
        return fp;
    }
}

void futex_free(uint64 uaddr) {
    hash_delete(&futex_map, (void *)&uaddr);
}

int futex_wait(uint64 uaddr, uint val, struct timespec *ts) {
    uint u_val;
    struct proc *p = proc_current();
    if (copyin(p->mm->pagetable, (char *)&u_val, uaddr, sizeof(uint)) < 0) {
        return -1;
    }

    struct futex *fp = get_futex(uaddr);
    acquire(&fp->lock);
    if (u_val == val) {
        struct tcb *t = thread_current();

        acquire(&t->lock);
        TCB_Q_changeState(t, TCB_SLEEPING);
        Queue_push_back_atomic(&fp->waiting_queue, t);
        release(&fp->lock);

        if (ts != NULL) {
            t->time_out = TIMESEPC2NS((*ts));
        }
        t->wait_chan_entry = &fp->waiting_queue; // !!!!!

        int ret = thread_sched();

        release(&t->lock);
        return ret;
    } else {
        release(&fp->lock);
        return 0;
    }
}

int futex_wakeup(uint64 uaddr, int nr_wake) {
    return 0;
}

int futex_requeue(uint64 uaddr1, int nr_wake, uint64 uaddr2) {
    return 0;
}

// global futex struct table (similar to inode table)
// struct futex futex_table[FUTEX_NUM];

// void futex_table_init() {
//     struct futex *entry;
//     for (entry = futex_table; entry < &futex_table[FUTEX_NUM]; entry++) {
//         initlock(&entry->lock, "futex lock");
//         Queue_init(&entry->waiting_queue, "futex queue lock", TCB_WAIT_QUEUE);
//         entry->uaddr = 0;
//         entry->valid = 0;
//     }
// }

// struct futex *futex_init(uint64 uaddr) {
//     struct futex *entry;
//     for (entry = futex_table; entry < &futex_table[FUTEX_NUM]; entry++) {
//         if (!entry->valid) {
//             entry->valid = 1;
//             entry->uaddr = uaddr;
//             return entry;
//         }
//     }
//     return NULL;
// }

// struct futex *futex_get(uint64 uaddr) {
//     struct futex *entry;
//     for (entry = futex_table; entry < &futex_table[FUTEX_NUM]; entry++) {
//         if (entry->valid && entry->uaddr == uaddr) {
//             return entry;
//         }
//     }
//     return NULL;
// }

// void futex_clear(uint64 uaddr) {
//     struct futex *entry;
//     for (entry = futex_table; entry < &futex_table[FUTEX_NUM]; entry++) {
//         if (entry->valid && entry->uaddr == uaddr) {
//             entry->valid = 0;
//             entry->uaddr = 0;
//             return;
//         }
//     }
// }

// int futex_wait(uint64 uaddr, uint val, struct timespec ts) {
//     struct futex *fp;
//     if ((fp = futex_init(uaddr)) == NULL) {
//         printfRed("no emptry entry for futex table\n");
//         return -1;
//     }
//     struct tcb *t = thread_current();
//     if (t) {
//         do_sleep_ns(t, ts);
//     }

//     return 0;
// }

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

int do_futex(uint64 uaddr, int op, uint32 val, struct timespec *ts,
             uint64 uaddr2, uint32 val2, uint32 val3) {
    int cmd = op & FUTEX_CMD_MASK;

    // struct proc *p = proc_current();

    int ret;
    switch (cmd) {
    case FUTEX_WAIT:
        ret = futex_wait(uaddr, val, ts);
        if (ret == -1)
            return -1;
        break;

    case FUTEX_WAKE:

        break;

    case FUTEX_REQUEUE:

        break;

    default:
        panic("do_futex : error\n");
        break;
    }

    return 0;
}