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

struct futex *get_futex(uint64 uaddr, int assert) {
    struct hash_node *node;
    if ((node = hash_lookup(&futex_map, (void *)&uaddr, NULL, 1)) != NULL) { // release it
        return (struct futex *)(node->value);
    } else {
        if (assert == 1) {
            return NULL;
            // panic("get_futex : error\n");
        }
        struct futex *fp = (struct futex *)kmalloc(sizeof(struct futex));
        if (fp == NULL) {
            panic("get_futex : no free space\n");
        }
        futex_init(fp, "futex");
        hash_insert(&futex_map, (void *)&uaddr, (void *)fp);
        return fp;
    }
}

// remember to release its memory
void futex_free(uint64 uaddr) {
    hash_delete(&futex_map, (void *)&uaddr);
}

int futex_wait(uint64 uaddr, uint val, struct timespec *ts) {
    uint u_val;
    struct proc *p = proc_current();
    if (copyin(p->mm->pagetable, (char *)&u_val, uaddr, sizeof(uint)) < 0) {
        return -1;
    }

    struct futex *fp = get_futex(uaddr, 0); // without assert
    acquire(&fp->lock);
    if (u_val == val) {
        struct tcb *t = thread_current();

        acquire(&t->lock);
        TCB_Q_changeState(t, TCB_SLEEPING);
        Queue_push_back(&fp->waiting_queue, t);
        release(&fp->lock);

        if (ts != NULL) {
            t->time_out = TIMESEPC2NS((*ts));
        }
        t->wait_chan_entry = &fp->waiting_queue; // !!!!!

        int ret = thread_sched();

        release(&t->lock);
        return ret ? -1 : 0;
    } else {
        release(&fp->lock);
        return 0;
    }
}

int futex_wakeup(uint64 uaddr, int nr_wake) {
    struct futex *fp = get_futex(uaddr, 1); // with assert
    struct tcb *t = NULL;
    int ret = 0;

    acquire(&fp->lock);
    while (!Queue_isempty(&fp->waiting_queue) && ret < nr_wake) {
        t = (struct tcb *)Queue_provide_atomic(&fp->waiting_queue, 1); // remove it

        if (t == NULL)
            panic("futex wakeup : no waiting queue");
        if (t->state != TCB_SLEEPING) {
            printf("%s\n", fp->waiting_queue.name);
            printf("%s\n", t->state);
            panic("futex wakeup : this thread is not sleeping");
        }
        acquire(&t->lock);
        ASSERT(t->wait_chan_entry != NULL);
        t->wait_chan_entry = NULL;
        TCB_Q_changeState(t, TCB_RUNNABLE);
        release(&t->lock);
        ret++;
    }
    release(&fp->lock);

    if (Queue_isempty_atomic(&fp->waiting_queue)) {
        futex_free(uaddr); // !!! avoid memory leak
    }
    return ret;
}

// 唤醒最多nr_wake个在uaddr1队列上等待的线程，其余的线程阻塞在uaddr2的队列
int futex_requeue(uint64 uaddr1, int nr_wake, uint64 uaddr2, int nr_requeue) {
    int nr_wake1 = futex_wakeup(uaddr1, nr_wake);
    printfRed("futex_requeue : has wakeup %d threads\n", nr_wake1); // debug

    struct futex *fp_old = get_futex(uaddr1, 1); // with assert
    if (fp_old == NULL) {
        return 0;
    }

    ASSERT(!Queue_isempty_atomic(&fp_old->waiting_queue));

    struct futex *fp_new = get_futex(uaddr2, 0); // without assert
    struct tcb *t = NULL;
    int ret = 0;

    while (!Queue_isempty_atomic(&fp_old->waiting_queue) && ret < nr_requeue) {
        t = (struct tcb *)Queue_provide_atomic(&fp_old->waiting_queue, 1); // remove it
        if (t == NULL)
            panic("futex wakeup : no waiting queue");
        if (t->state != TCB_SLEEPING) {
            printf("%s\n", fp_old->waiting_queue.name);
            printf("%s\n", t->state);
            panic("futex wakeup : this thread is not sleeping");
        }
        Queue_push_back_atomic(&fp_new->waiting_queue, (void *)t); // move the rest of threads to new queue
        ret++;
    }

    return ret;
}

int do_futex(uint64 uaddr, int op, uint32 val, struct timespec *ts,
             uint64 uaddr2, uint32 val2, uint32 val3) {
    int cmd = op & FUTEX_CMD_MASK;

    int ret = -1;
    switch (cmd) {
    case FUTEX_WAIT:
        ret = futex_wait(uaddr, val, ts);
        break;

    case FUTEX_WAKE:
        ret = futex_wakeup(uaddr, val);
        break;

    case FUTEX_REQUEUE:
        ret = futex_requeue(uaddr, val, uaddr2, val2); // must use val2 as a limit of requeue
        break;

    default:
        panic("do_futex : error\n");
        break;
    }
    return ret;
}