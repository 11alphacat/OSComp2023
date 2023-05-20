#include "atomic/spinlock.h"
#include "proc/tcb_queue.h"
#include "proc/wait_queue.h"
#include "proc/sched.h"
#include "proc/futex.h"
#include "list.h"

struct futex *getfutex(int *uaddr) {
    struct futex_cond *p = futex_entry(uaddr, struct futex_cond, value);
    return p->fp;
};

void futex_wait(int *uaddr, int val) {
    struct futex *fp = getfutex(uaddr);
    acquire(&fp->lock);
    if (*uaddr == val) {
        struct tcb *t = thread_current();

        acquire(&t->lock);
        TCB_Q_changeState(t, TCB_SLEEPING);
        Waiting_Q_push_back(&fp->waiting_queue, t);
        release(&fp->lock);

        thread_sched();
        release(&t->lock);

    } else {
        release(&fp->lock);
    }
}

void futex_signal(int *uaddr) {
    struct futex *fp = getfutex(uaddr);
    struct tcb *t;

    acquire(&fp->lock);

    if (!Waiting_Q_isempty_atomic(&fp->waiting_queue)) {
        t = Waiting_Q_provide(&fp->waiting_queue);
        if (t == NULL)
            panic("cond signal : this cond has no object waiting queue");
        if (t->state != TCB_SLEEPING) {
            printf("%s\n", t->state);
            panic("cond signal : this thread is not sleeping");
        }
        acquire(&t->lock);
        TCB_Q_changeState(t, TCB_RUNNABLE);
        release(&t->lock);
    }

    release(&fp->lock);
}

void futex_cond_wait(struct futex_cond *cond, struct spinlock *mutex) {
    uint local = cond->value;
    release(mutex);
    futex_wait(&cond->value, local);
    acquire(mutex);
}

void futex_cond_signal(struct futex_cond *cond) {
    cond->value += 1;
    futex_signal(&cond->value);
}
