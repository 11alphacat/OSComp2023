#ifndef __PCB_THREAD_H__
#define __PCB_THREAD_H__
#include "kernel/kthread.h"
#include "atomic/spinlock.h"
#include "list.h"
#include "memory/allocator.h"
#include "proc/pcb_life.h"
#include "memory/memlayout.h"
#include "riscv.h"

enum thread_status { TCB_RUNNABLE,
                     TCB_RUNNING };
typedef enum thread_status thread_status_t;
extern struct proc proc[NPROC];

struct tcb {
    spinlock_t lock;
    thread_status_t t_status;
    struct proc *p;
    uint64 kstack;
    int exit_status;
    char is_detached;
    struct list_head threads; // thread list
};

// 1. allocate a new thread
struct tcb *alloc_thread(struct proc *p);

// 2. push back the thread to the proc
void TCB_Q_push_back(struct tcb *tcb_p, struct proc *p);

// 3. remove the thread from the proc
void TCB_Q_remove(struct tcb *tcb_p);

// 4. create
int pthread_create(uint64 stack);

// 5. exit
// void pthread_exit();

// // 3. join
// int pthread_join();

// // 4. detach
// int pthread_detach();

#endif