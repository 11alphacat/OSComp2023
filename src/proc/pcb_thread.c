#include "kernel/cpu.h"
#include "proc/sched.h"
#include "proc/pcb_thread.h"
#include "proc/tcb_queue.h"
#include "kernel/trap.h"
#include "list.h"
#include "debug.h"

struct tcb thread[NTCB];
char tcb_lock_name[NTCB][10];

extern TCB_Q_t unused_t_q, runnable_t_q, sleeping_t_q;
extern TCB_Q_t *STATES[TCB_STATEMAX];

atomic_t next_tid;
atomic_t count_tid;

extern struct proc *initproc;

#define alloc_tid (atomic_inc_return(&next_tid))
#define cnt_tid_inc (atomic_inc_return(&count_tid))
#define cnt_tid_dec (atomic_dec_return(&count_tid))

// tcb init
void tcb_init(void) {
    struct tcb *t;
    atomic_set(&next_tid, 1);
    atomic_set(&count_tid, 0);

    TCB_Q_ALL_INIT();
    for (int i = 0; i < NTCB; i++) {
        t = thread + i;
        initlock(&t->lock, tcb_lock_name[i]); // init its spinlock
        sema_init(&t->sem_wait_chan_self, 0, "proc_sema_chan_self");

        t->state = TCB_UNUSED;
        t->kstack = KSTACK((int)(t - thread));
        TCB_Q_push_back(&unused_t_q, t);
    }
    return;
}

// the kernel thread in current cpu
struct tcb *thread_current(void) {
    push_off();
    struct thread_cpu *c = t_mycpu();
    struct tcb *thread = c->thread;
    pop_off();
    return thread;
}

// allocate a new kernel thread
struct tcb *alloc_thread(void) {
    struct tcb *t;

    t = TCB_Q_provide(&unused_t_q, 1);
    if (t == NULL)
        return 0;
    acquire(&t->lock);

    // spinlock and threads list head

    INIT_LIST_HEAD(&t->threads);

    t->tid = alloc_tid;
    cnt_tid_dec;

    TCB_Q_changeState(t, TCB_USED);

    // Allocate a trapframe page.
    if ((t->trapframe = (struct trapframe *)kalloc()) == 0) {
        free_thread(t);
        release(&t->lock);
        return 0;
    }

    // Set up new context to start executing at forkret, which returns to user space.
    memset(&t->context, 0, sizeof(t->context));
    t->context.ra = (uint64)thread_forkret;
    t->context.sp = t->kstack + PGSIZE;

    return t;
}

// free a thread
void free_thread(struct tcb *t) {
    if (t->trapframe)
        kfree((void *)t->trapframe);
    t->trapframe = 0;
    t->tid = 0;
    t->name[0] = 0;
    t->exit_status = 0;
    t->p = 0;

    TCB_Q_changeState(t, TCB_UNUSED);
}

void proc_join_thread(struct proc *p, struct tcb *t) {
    acquire(&p->tg->lock);
    if (p->tg->group_leader == NULL) {
        p->tg->group_leader = t;
    }
    list_add_tail(&t->threads, &p->tg->threads);
    p->tg->thread_cnt++;
    t->tidx = p->tg->thread_idx;
    t->p = p;
    release(&p->tg->lock);
}

int proc_release_thread(struct proc *p, struct tcb *t) {
    acquire(&p->tg->lock);
    p->tg->thread_cnt--;
    list_del_reinit(&t->threads);
    if (p->tg->thread_cnt == 0) {
        if (p->tg->group_leader != t) {
            panic("main thread exited early");
        }
        release(&p->tg->lock);
        return 1;
    }
    release(&p->tg->lock);
    return 0;
}

void proc_release_all_thread(struct proc *p) {
    struct tcb *t_cur = NULL;
    struct tcb *t_tmp = NULL;
    acquire(&p->tg->lock);
    list_for_each_entry_safe(t_cur, t_tmp, &p->tg->threads, threads) {
        acquire(&t_cur->lock);
        list_del_reinit(&t_cur->threads);
        free_thread(t_cur);
        release(&t_cur->lock);
    }
    p->tg->thread_cnt = 0;
    release(&p->tg->lock);
}

void proc_wakeup_all_thread(struct proc *p) {
    struct tcb *t_cur = NULL;
    struct tcb *t_tmp = NULL;
    acquire(&p->tg->lock);
    list_for_each_entry_safe(t_cur, t_tmp, &p->tg->threads, threads) {
        acquire(&t_cur->lock);
        if (t_cur->state == TCB_SLEEPING) {
            TCB_Q_changeState(t_cur, TCB_RUNNABLE);
        }
        release(&t_cur->lock);
    }
    release(&p->tg->lock);
}

// thread exit
// similar to exit of proc
void exit_thread(int status) {
    struct tcb *t = thread_current();
    struct proc *p = proc_current();

    t->exit_status = status;
    memset(&t->context, 0, sizeof(t->context));
    int last = proc_release_thread(p, t);

    free_thread(t);
    cnt_tid_dec;
    if (last == 0) {
        do_exit(0);
    } else {
        thread_sched();
        panic("existed thread is scheduled");
    }
    return;
}

void tginit(struct thread_group *tg) {
    initlock(&tg->lock, "thread group lock");
    tg->group_leader = NULL;
    tg->thread_cnt = 0;
    tg->thread_idx = 0;
    INIT_LIST_HEAD(&tg->threads);
}

// // thread join
// // similar to wait
// int join_thread(struct tcb* thread, int* status) {
//     while(thread->state!=TCB_ZOMBIE) {
//         thread_yield();
//     }
//     *status = thread->exit_status;
//     free_thread(thread);
// }