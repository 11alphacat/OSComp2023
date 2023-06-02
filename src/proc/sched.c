#include "common.h"
#include "proc/pcb_queue.h"
#include "proc/sched.h"
#include "atomic/spinlock.h"
#include "lib/riscv.h"
#include "kernel/cpu.h"
#include "proc/sched.h"
#include "kernel/trap.h"
#include "proc/pcb_life.h"
#include "proc/pcb_thread.h"
#include "proc/tcb_queue.h"
#include "debug.h"

// UNUSED, SLEEPING, ZOMBIE
PCB_Q_t unused_p_q, used_p_q, zombie_p_q;
PCB_Q_t *STATES[PCB_STATEMAX] = {
    [PCB_UNUSED] & unused_p_q,
    [PCB_USED] & used_p_q,
    [PCB_ZOMBIE] & zombie_p_q};

// UNUSED, USED, SLEEPING, RUNNABLE
// the running thread is on the cpu, so we do not need it (running queue).
// create a mapping from emurate state to the address of queue.
TCB_Q_t unused_t_q, used_t_q, runnable_t_q, sleeping_t_q;
TCB_Q_t *T_STATES[TCB_STATEMAX] = {
    [TCB_UNUSED] & unused_t_q,
    [TCB_USED] & used_t_q,
    [TCB_RUNNABLE] & runnable_t_q,
    [TCB_SLEEPING] & sleeping_t_q};

extern struct proc proc[NPROC];
extern struct tcb thread[NTCB];

void PCB_Q_ALL_INIT() {
    PCB_Q_init(&unused_p_q, "PCB_UNUSED");
    PCB_Q_init(&used_p_q, "PCB_USED");
    PCB_Q_init(&zombie_p_q, "PCB_ZOMBIE");
}

void TCB_Q_ALL_INIT() {
    TCB_Q_init(&unused_t_q, "TCB_UNUSED");
    TCB_Q_init(&used_t_q, "TCB_USED");
    TCB_Q_init(&runnable_t_q, "TCB_RUNNABLE");
    TCB_Q_init(&sleeping_t_q, "TCB_SLEEPING");
}

void PCB_Q_changeState(struct proc *p, enum procstate state_new) {
    PCB_Q_t *pcb_q_new = STATES[state_new];
    PCB_Q_t *pcb_q_old = STATES[p->state];

    acquire(&pcb_q_old->lock);
    PCB_Q_remove(p);
    release(&pcb_q_old->lock);

    acquire(&pcb_q_new->lock);
    PCB_Q_push_back(pcb_q_new, p);
    release(&pcb_q_new->lock);

    p->state = state_new;
    return;
}

void TCB_Q_changeState(struct tcb *t, enum thread_state state_new) {
    TCB_Q_t *tcb_q_new = T_STATES[state_new];
    TCB_Q_t *tcb_q_old = T_STATES[t->state];

    if (t->state != TCB_RUNNING) {
        acquire(&tcb_q_old->lock);
        TCB_Q_remove(t);
        release(&tcb_q_old->lock);
    } else {
        TCB_Q_remove(t);
    }
    acquire(&tcb_q_new->lock);
    TCB_Q_push_back(tcb_q_new, t);
    release(&tcb_q_new->lock);

    t->state = state_new;
    return;
}

void thread_yield(void) {
    struct tcb *t = thread_current();
    acquire(&t->lock);

    TCB_Q_changeState(t, TCB_RUNNABLE);

    thread_sched();
    release(&t->lock);
}

void thread_sched(void) {
    int intena;
    struct tcb *thread = thread_current();

    if (!holding(&thread->lock))
        panic("sched thread->lock");
    if (t_mycpu()->noff != 1) {
        panic("sched locks");
    }
    if (thread->state == TCB_RUNNING)
        panic("sched running");
    if (intr_get())
        panic("sched interruptible");

    intena = t_mycpu()->intena;
    swtch(&thread->context, &t_mycpu()->context);
    t_mycpu()->intena = intena;
}

void thread_scheduler(void) {
    struct tcb *t;
    struct thread_cpu *c = t_mycpu();

    c->thread = 0;
    for (;;) {
        // Avoid deadlock by ensuring that devices can interrupt.
        intr_on();
        t = TCB_Q_provide(&runnable_t_q, 1);
        if (t == NULL)
            continue;
        acquire(&t->lock);
        t->state = TCB_RUNNING;
        c->thread = t;
        swtch(&c->context, &t->context);
        c->thread = 0;
        release(&t->lock);
    }
}