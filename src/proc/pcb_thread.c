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
    // memset(&(t->sig_trapframe), 0, sizeof(t->sig_trapframe));
    t->sig_pending_cnt = 0;

    if ((t->sig = (struct sighand *)kalloc()) == 0) {
        panic("no space for sighand\n");
    }
    sig_empty_set(&t->blocked);
    sighandinit(t->sig);
    sigpending_init(&(t->pending));

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

    if (t->sig) // bug!
        kfree((void *)t->sig);

    t->trapframe = 0;
    t->tid = 0;
    t->name[0] = 0;
    t->exit_status = 0;
    t->p = 0;

    t->sig_pending_cnt = 0;
    t->sig_ing = 0;
    signal_queue_flush(&t->pending);

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

// send signal to all threads of proc p
void proc_sendsignal_all_thread(struct proc *p, sig_t signo, int opt) {
    struct tcb *t_cur = NULL;
    struct tcb *t_tmp = NULL;
    siginfo_t info;
    acquire(&p->tg->lock);
    list_for_each_entry_safe(t_cur, t_tmp, &p->tg->threads, threads) {
        signal_info_init(signo, &info, opt);

        acquire(&t_cur->lock);
        thread_send_signal(t_cur, &info);
        release(&t_cur->lock);
    }
    release(&p->tg->lock);
    if (signo == SIGKILL || signo == SIGSTOP) {
        proc_setkilled(p);
    }
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

// set killed state for thread
void thread_setkilled(struct tcb *t) {
    acquire(&t->lock);
    t->killed = 1;
    release(&t->lock);
}

// is thread killed ?
int thread_killed(struct tcb *t) {
    int k;

    acquire(&t->lock);
    k = t->killed;
    release(&t->lock);
    return k;
}

void tginit(struct thread_group *tg) {
    initlock(&tg->lock, "thread group lock");
    tg->group_leader = NULL;
    tg->thread_cnt = 0;
    tg->thread_idx = 0;
    INIT_LIST_HEAD(&tg->threads);
}

void sighandinit(struct sighand *sig) {
    initlock(&sig->siglock, "signal handler lock");
    atomic_set(&(sig->count), 0);
    // memset the signal handler???
}

// send signal to thread (wakeup tcb sleeping)
void thread_send_signal(struct tcb *t_cur, siginfo_t *info) {
    signal_send(info, t_cur);

    // wakeup thread sleeping of proc p
    if (info->si_signo == SIGKILL || info->si_signo == SIGSTOP) {
        if (t_cur->state == TCB_SLEEPING) {
            Waiting_Q_remove_atomic(&cond_ticks.waiting_queue, t_cur); // bug
            TCB_Q_changeState(t_cur, TCB_RUNNABLE);
        }
    }
#ifdef __DEBUG_PROC__
    printfCYAN("tkill : kill thread %d, signo = %d\n", t_cur->tid, info->si_signo); // debug
#endif
    return;
}

// find the tcb* given tid
struct tcb *find_get_tid(tid_t tid) {
    struct tcb *t;
    for (t = thread; t < &thread[NTCB]; t++) {
        acquire(&t->lock);
        if (t->tid == tid) {
            return t;
        }
        release(&t->lock);
    }
    return NULL;
}

// find tcb given pid and tidx
struct tcb *find_get_tidx(int pid, int tidx) {
    // find proc given pid
    struct proc *p;
    if ((p = find_get_pid(pid)) == NULL)
        return NULL;
    release(&p->lock);

    // find thread given tidx
    struct tcb *t_cur = NULL;
    struct tcb *t_tmp = NULL;
    acquire(&p->tg->lock);
    list_for_each_entry_safe(t_cur, t_tmp, &p->tg->threads, threads) {
        acquire(&t_cur->lock);
        // tidx from 0, but tid == 0 stands for invalid thread
        if (t_cur->tidx + 1 == tidx) {
            release(&p->tg->lock);
            return t_cur;
        }
        release(&t_cur->lock);
    }
    release(&p->tg->lock);

    return NULL;
}

void do_tkill(struct tcb *t, sig_t signo) {
    siginfo_t info;
    signal_info_init(signo, &info, 0);
    acquire(&t->lock);
    thread_send_signal(t, &info);
    release(&t->lock);
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