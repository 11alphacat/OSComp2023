#include "proc/pcb_thread.h"
#include "list.h"
#include "debug.h"

// allocate a new thread
struct tcb *alloc_thread(struct proc *p) {
    struct tcb *tcb_new = kalloc();
    ASSERT(tcb_new != 0);
    initlock(&tcb_new->lock, "tcb_lock");
    INIT_LIST_HEAD(&tcb_new->threads);
    tcb_new->t_status = TCB_RUNNABLE;
    tcb_new->p = p;
    tcb_new->kstack = KSTACK((int)(p - proc));
    tcb_new->exit_status = 0;
    tcb_new->is_detached = 0;
    return tcb_new;
}

// push a new thread to the back of the thread group
void TCB_Q_push_back(struct tcb *tcb_p, struct proc *p) {
    list_add_tail(&(tcb_p->threads), &(p->thread_group));
}

// move tcb from its proc
void TCB_Q_remove(struct tcb *tcb_p) {
    list_del(&tcb_p->threads);
    INIT_LIST_HEAD(&(tcb_p->threads));
}

// create a new thread
int pthread_create(uint64 stack) {
    struct proc *p = current();
    struct tcb *new_thread = alloc_thread(p);
    new_thread->kstack = stack;
    TCB_Q_push_back(new_thread, p);
    return 1;
}
