#include "proc/pcb_queue.h"
#include "proc/sched.h"
#include "atomic/spinlock.h"
#include "riscv.h"
#include "kernel/cpu.h"
#include "proc/sched.h"
#include "kernel/trap.h"

// UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE
PCB_Q_t unused_q, used_q, running_q, runnable_q, sleeping_q, zombie_q;

extern struct proc proc[NPROC];

void PCB_Q_ALL_INIT(){
    PCB_Q_init(&unused_q, "UNUSED");
    PCB_Q_init(&used_q, "USED");
    PCB_Q_init(&running_q, "RUNNING");
    PCB_Q_init(&runnable_q, "RUNNABLE");
    PCB_Q_init(&sleeping_q, "SLEEPING");
    PCB_Q_init(&zombie_q, "ZOMBIE");
}

// Give up the CPU for one scheduling round.
void yield(void) {
    struct proc *p = myproc();
    acquire(&p->lock);
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
    struct proc *p = myproc();

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

        for (p = proc; p < &proc[NPROC]; p++) {
            acquire(&p->lock);
            if (p->state == RUNNABLE) {
                // Switch to chosen process.  It is the process's job
                // to release its lock and then reacquire it
                // before jumping back to us.
                p->state = RUNNING;
                c->proc = p;
                swtch(&c->context, &p->context);

                // Process is done running for now.
                // It should have changed its p->state before coming back.
                c->proc = 0;
            }
            release(&p->lock);
        }
    }
}