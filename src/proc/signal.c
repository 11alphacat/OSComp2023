#include "proc/signal.h"
#include "proc/pcb_life.h"
#include "proc/sched.h"

extern struct proc proc[NPROC];
extern PCB_Q_t unused_q, used_q, runnable_q, sleeping_q, zombie_q;

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int kill(int pid) {
    struct proc *p;

    if((p = find_get_pid(pid))==NULL)
        return -1;
        
    p->killed = 1;
    if (p->state == SLEEPING) {
        // Wake process from sleep().
        PCB_Q_changeState(p, RUNNABLE);
        p->state = RUNNABLE;
    }
    release(&p->lock);
    return 0;
}

void setkilled(struct proc *p) {
    acquire(&p->lock);
    p->killed = 1;
    release(&p->lock);
}

int killed(struct proc *p) {
    int k;

    acquire(&p->lock);
    k = p->killed;
    release(&p->lock);
    return k;
}
