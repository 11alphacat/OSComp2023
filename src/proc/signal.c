#include "proc/pcb_life.h"
#include "proc/sched.h"
#include "proc/signal.h"
#include "errno.h"
#include "debug.h"

extern struct proc proc[NPROC];
extern PCB_Q_t unused_q, used_q, runnable_q, sleeping_q, zombie_q;

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int kill(int pid) {
    struct proc *p;

    if ((p = find_get_pid(pid)) == NULL)
        return -1;

    p->killed = 1;

#ifdef __DEBUG_PROC__
    printfCYAN("kill : kill %d\n", p->pid); // debug
#endif

    if (p->state == SLEEPING) {
        // Wake process from sleep().
        PCB_Q_changeState(p, RUNNABLE);
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

int do_kill(int sig, siginfo_t *info, int pid) {
    if (!pid) {
        // pid == 0

    } else if (pid == -1) {
        // pid == -1

    } else if (pid < 0) {
        // pid <0

    } else {
        // pid >0
        return kill_proc_info(sig, info, pid);
    }
    return 1;
}

int kill_proc_info(int sig, siginfo_t *info, pid_t pid) {
    int error = 0;
    struct proc *p;
    p = find_get_pid(pid);
    acquire(&p->lock);
    if (p != NULL)
        error = send_sig_info(sig, info, p);
    release(&p->lock);
    return error;
}

int send_sig_info(int sig, siginfo_t *info, struct proc *p) {
    if (!valid_signal(sig))
        return -EINVAL;
    // uint64 flags;
    return 1;
}
