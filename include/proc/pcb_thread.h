#ifndef __PCB_THREAD_H__
#define __PCB_THREAD_H__
#include "kernel/kthread.h"
#include "memory/allocator.h"
#include "proc/pcb_life.h"
#include "memory/memlayout.h"
#include "riscv.h"
#include "atomic/spinlock.h"
#include "list.h"
#include "common.h"

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
    spinlock_t lock;          // spinlock
    tid_t tgid;               // thread group id
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

    int tidx;

    // exit status
    int exit_status; // Exit status to be returned to parent's wait

    // tcb state queue
    struct list_head state_list; // its state queue

    // signal
    int sigpending;                   // have signal?
    struct signal_struct *sig;        // signal
    sigset_t blocked;                 // the blocked signal
    struct sigpending pending;        // pending (private)
    struct sigpending shared_pending; // pending (shared)

    // kernel stack 、trapframe and context
    uint64 kstack;               // kernel stack
    struct trapframe *trapframe; // data page for trampoline.S
    struct context context;      // swtch() here to run thread

    // thread name
    char name[16]; // Thread name (debugging)

    // thread list
    struct list_head threads; // thread list

    // is detached ?
    uint is_detached;

    void *chan;                 // If non-zero, sleeping on chan
    struct list_head wait_list; // waiting queue (semaphore)

    struct semaphore sem_wait_chan_parent;
    struct semaphore sem_wait_chan_self;
};

void tcb_init(void);
struct tcb *thread_current(void);
struct tcb *alloc_thread(void);
void free_thread(struct tcb *t);
void exit_thread(int status);
void proc_join_thread(struct proc *p, struct tcb *t);
int proc_release_thread(struct proc *p, struct tcb *t);
void proc_release_all_thread(struct proc *p);
void proc_wakeup_all_thread(struct proc *p);
void thread_forkret(void);
void tginit(struct thread_group* tg);


#endif