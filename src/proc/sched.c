#include "common.h"
#include "proc/pcb_queue.h"
#include "proc/sched.h"
#include "atomic/spinlock.h"
#include "riscv.h"
#include "kernel/cpu.h"
#include "proc/sched.h"
#include "kernel/trap.h"
#include "proc/pcb_life.h"

// UNUSED, USED, SLEEPING, RUNNABLE, ZOMBIE
PCB_Q_t unused_q, used_q, runnable_q, sleeping_q, zombie_q;
// the running process is on the cpu, so we do not need it (running queue).
// create a mapping from emurate state to the address of queue
PCB_Q_t *STATES[STATEMAX] = {
    [UNUSED] & unused_q,
    [USED] & used_q,
    [RUNNABLE] & runnable_q,
    [SLEEPING] & sleeping_q,
    [ZOMBIE] & zombie_q};

extern struct proc proc[NPROC];

void PCB_Q_ALL_INIT() {
    PCB_Q_init(&unused_q, "UNUSED");
    PCB_Q_init(&used_q, "USED");
    PCB_Q_init(&runnable_q, "RUNNABLE");
    PCB_Q_init(&sleeping_q, "SLEEPING");
    PCB_Q_init(&zombie_q, "ZOMBIE");
}

void PCB_Q_changeState(struct proc *p, enum procstate state_new) {
    PCB_Q_t *pcb_q_new = STATES[state_new];

    PCB_Q_remove(p);
    acquire(&pcb_q_new->lock);
    PCB_Q_push_back(pcb_q_new, p);
    release(&pcb_q_new->lock);
    return;
}

// Give up the CPU for one scheduling round.
void yield(void) {
    struct proc *p = current();
    acquire(&p->lock);

    // runnable queue
    PCB_Q_changeState(p, RUNNABLE);
    p->state = RUNNABLE;

    sched();
    release(&p->lock);
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void) {
    int intena;
    struct proc *p = current();

    if (!holding(&p->lock))
        panic("sched p->lock");
    if (mycpu()->noff != 1)
        panic("sched locks");
    if (p->state == RUNNING)
        panic("sched running");
    if (intr_get())
        panic("sched interruptible");

    intena = mycpu()->intena;
    swtch(&p->context, &mycpu()->context);
    mycpu()->intena = intena;
}

#include "debug.h"
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void scheduler(void) {
    struct proc *p;
    struct cpu *c = mycpu();
    c->proc = 0;
    for (;;) {
        // Avoid deadlock by ensuring that devices can interrupt.
        intr_on();
        p = PCB_Q_provide(&runnable_q);
        if (p == NULL)
            continue;
        acquire(&p->lock);
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);
        c->proc = 0;
        release(&p->lock);
    }
}