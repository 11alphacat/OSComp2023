#include "proc/pcb_life.h"
#include "proc/pcb_thread.h"
#include "atomic/cond.h"
#include "proc/sched.h"
#include "kernel/cpu.h"
#include "debug.h"

extern struct proc proc[NPROC];
extern struct tcb thread[NTCB];

extern PCB_Q_t unused_p_q, used_p_q, zombie_p_q;
extern TCB_Q_t unused_t_q, runnable_t_q, sleeping_t_q;

// init
void cond_init(struct cond *cond, char *name) {
    Waiting_Q_init(&cond->waiting_queue, name);
}

// wait
void cond_wait(struct cond *cond, struct spinlock *mutex) {
    // struct proc *p = proc_current();
    struct tcb *t = thread_current();

    acquire(&t->lock);

    TCB_Q_changeState(t, TCB_SLEEPING);

    Waiting_Q_push_back(&cond->waiting_queue, t);

    // TODO : modify it to futex(ref to linux)
    release(mutex);

    thread_sched();

    // Reacquire original lock.
    release(&t->lock);
    acquire(mutex);
}

// just signal a object!!!
void cond_signal(struct cond *cond) {
    struct tcb *t;

    if (!Waiting_Q_isempty_atomic(&cond->waiting_queue)) {
        t = Waiting_Q_provide(&cond->waiting_queue);

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
}

// signal all object!!!
void cond_broadcast(struct cond *cond) {
    struct tcb *t;
    while (!Waiting_Q_isempty_atomic(&cond->waiting_queue)) {
        t = Waiting_Q_provide(&cond->waiting_queue);

        if (t == NULL)
            panic("cond signal : this cond has no object waiting queue");
        if (t->state != TCB_SLEEPING) {
            printf("%s\n", cond->waiting_queue.name);
            printf("%s\n", t->state);
            panic("cond broadcast : this thread is not sleeping");
        }
        // if (t->state != TCB_SLEEPING) {
        //     ASSERT(t->state == TCB_UNUSED);
        //     // printf("cond broadcast : this thread is not sleeping\n");
        //     continue;
        // }
        acquire(&t->lock);
        TCB_Q_changeState(t, TCB_RUNNABLE);
        release(&t->lock);
    }
}

// // Atomically release lock and sleep on chan.
// // Reacquires lock when awakened.
// void sleep(void *chan, struct spinlock *lk)
// {
//     struct tcb *t = thread_current();

//     acquire(&t->lock);

//     release(lk);

//     // Go to sleep.
//     t->chan = chan;

//     TCB_Q_changeState(t, TCB_SLEEPING);

//     thread_sched();

//     // Tidy up.
//     t->chan = 0;

//     // Reacquire original lock.
//     release(&t->lock);
//     acquire(lk);
// }

// // Wake up all processes sleeping on chan.
// // Must be called without any p->lock.
// void wakeup(void *chan)
// {
//     struct tcb *t = thread_current();

//     for(t = thread; t < &thread[NTCB]; t++) {
//         if(t != thread_current()){
//             acquire(&t->lock);
//             if(t->state == TCB_SLEEPING && t->chan == chan) {
//                 TCB_Q_changeState(t, TCB_RUNNABLE);
//             }
//             release(&t->lock);
//         }
//     }
// }