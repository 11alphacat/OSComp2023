#include "proc/pcb_life.h"
#include "proc/cond.h"
#include "proc/sched.h"
#include "debug.h"

extern struct proc proc[NPROC];
extern PCB_Q_t unused_q, used_q, runnable_q, sleeping_q, zombie_q;

// init
void cond_init(struct cond *cond, char *name) {
    Waiting_Q_init(&cond->waiting_queue, name);
}

// wait
void cond_wait(struct cond *cond, struct spinlock *mutex) {
    struct proc *p = current();

    acquire(&p->lock);

    PCB_Q_changeState(p, SLEEPING);
    p->state = SLEEPING;

    ASSERT(cond->waiting_queue.lock.locked!=1);

    // acquire(&cond->waiting_queue.lock);
    Waiting_Q_push_back(&cond->waiting_queue, p);
    // release(&cond->waiting_queue.lock);
    // TODO : modify it to futex(ref to linux)
    release(mutex);
    sched();

    // Reacquire original lock.
    release(&p->lock);
    acquire(mutex);
}

// just signal a object!!!
void cond_signal(struct cond *cond) {
    struct proc *p;
    if (!Waiting_Q_isempty(&cond->waiting_queue)) {
        p = Waiting_Q_provide(&cond->waiting_queue);
        if (p == NULL)
            panic("cond signal : this cond has no object waiting queue");
        if (p->state != SLEEPING){
            printf("%s\n",p->state);
            panic("cond signal : this proc is not sleeping");
        }
        acquire(&p->lock);
        PCB_Q_changeState(p, RUNNABLE);
        p->state = RUNNABLE;
        release(&p->lock);
    }
}

// signal all object!!!
void cond_broadcast(struct cond *cond) {
    struct proc *p;
    while (!Waiting_Q_isempty(&cond->waiting_queue)) {
        p = Waiting_Q_provide(&cond->waiting_queue);
        if (p == NULL)
            panic("cond signal : this cond has no object waiting queue");
        if (p->state != SLEEPING){
            printf("%s\n",p->state);
            panic("cond signal : this proc is not sleeping");
        }
        acquire(&p->lock);
        PCB_Q_changeState(p, RUNNABLE);
        p->state = RUNNABLE;
        release(&p->lock);
    }
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk) {
    struct proc *p = current();

    // Must acquire p->lock in order to
    // change p->state and then call sched.
    // Once we hold p->lock, we can be
    // guaranteed that we won't miss any wakeup
    // (wakeup locks p->lock),
    // so it's okay to release lk.

    acquire(&p->lock); // DOC: sleeplock1
    release(lk);

    // Go to sleep.
    p->chan = chan;
    PCB_Q_changeState(p, SLEEPING);
    p->state = SLEEPING;

    sched();

    // Tidy up.
    p->chan = 0;

    // Reacquire original lock.
    release(&p->lock);
    acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void wakeup(void *chan) {
    struct proc *p;

    for (p = proc; p < &proc[NPROC]; p++) {
        if (p != current()) {
            acquire(&p->lock);
            if (p->state == SLEEPING && p->chan == chan) {
                PCB_Q_changeState(p, RUNNABLE);
                p->state = RUNNABLE;
            }
            release(&p->lock);
        }
    }
}