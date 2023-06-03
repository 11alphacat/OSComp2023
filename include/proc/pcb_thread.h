#ifndef __PCB_THREAD_H__
#define __PCB_THREAD_H__
#include "kernel/kthread.h"
#include "memory/allocator.h"
#include "proc/pcb_life.h"
#include "memory/memlayout.h"
#include "lib/riscv.h"
#include "atomic/spinlock.h"
#include "lib/list.h"
#include "common.h"
#include "proc/signal.h"

#define NTCB_PER_PROC 10
#define NTCB ((NPROC) * (NTCB_PER_PROC))

enum thread_state { TCB_UNUSED,
                    TCB_USED,
                    TCB_RUNNABLE,
                    TCB_RUNNING,
                    TCB_SLEEPING,
                    TCB_STATEMAX };

typedef enum thread_state thread_state_t;

struct thread_group {
    spinlock_t lock; // spinlock
    tid_t tgid;      // thread group id
    int thread_idx;
    int thread_cnt;           // the count of threads
    struct list_head threads; // thread group
    struct tcb *group_leader; // its proc thread group leader
};

struct tcb {
    // spinlock
    spinlock_t lock; // spinlock protecting the thread control block

    thread_state_t state; // the state of thread
    struct proc *p;       // the proc pointer

    // thread id
    tid_t tid;

    // offset
    int tidx;

    // exit status
    int exit_status; // Exit status to be returned to parent's wait

    // thread : killed ?
    int killed; // If non-zero, have been killed

    // tcb state queue
    struct list_head state_list; // its state queue

    // signal
    int sig_pending_cnt;       // have signal?
    struct sighand *sig;       // signal
    sigset_t blocked;          // the blocked signal
    struct sigpending pending; // pending (private)
    sig_t sig_ing;

    // uint64 sas_ss_sp;
    // size_t sas_ss_size;
    // uint64 sas_ss_flags;
    // struct sigpending shared_pending; // pending (shared)

    // kernel stack, trapframe and context
    uint64 kstack;               // kernel stack
    struct trapframe *trapframe; // data page for trampoline.S
    struct context context;      // swtch() here to run thread

    // thread name
    char name[16]; // Thread name (debugging)

    // thread list
    struct list_head threads; // thread list

    // is detached ?
    uint is_detached;

    // void *chan;                 // If non-zero, sleeping on chan
    struct list_head wait_list; // waiting queue (semaphore)

    struct semaphore sem_wait_chan_parent;
    struct semaphore sem_wait_chan_self;

    /* CLONE_CHILD_SETTID: */
    int *set_child_tid;
    /* CLONE_CHILD_CLEARTID: */
    int *clear_child_tid;
};

void tcb_init(void);
struct tcb *thread_current(void);
struct tcb *alloc_thread(void);
void free_thread(struct tcb *t);
void exit_thread(int status);
int thread_killed(struct tcb *t);
void thread_setkilled(struct tcb *t);
void proc_join_thread(struct proc *p, struct tcb *t);
int proc_release_thread(struct proc *p, struct tcb *t);
void proc_release_all_thread(struct proc *p);
void proc_sendsignal_all_thread(struct proc *p, sig_t signo, int opt);
void thread_forkret(void);
void tginit(struct thread_group *tg);
void sighandinit(struct sighand *sig);
void thread_send_signal(struct tcb *t_cur, siginfo_t *info);
struct tcb *find_get_tid(tid_t tid);
struct tcb *find_get_tidx(int pid, int tidx);
void do_tkill(struct tcb *t, sig_t signo);
int do_sleep_ns(struct tcb *t, struct timespec ts);

#endif