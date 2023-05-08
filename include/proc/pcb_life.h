#ifndef __PCB_LIFE_H__
#define __PCB_LIFE_H__

#include "common.h"
#include "param.h"
#include "atomic/spinlock.h"
#include "kernel/kthread.h"
#include "list.h"
#include "proc/signal.h"
#include "atomic/semaphore.h"
#include "fs/fat/fat32_disk.h"

#define NPROC 32 // maximum number of processes
#define INIT_PID 1
#define SHELL_PID 2

struct _file;
struct _inode;
struct file;
struct inode;

enum pid_type {
    PIDTYPE_PID,  // 进程 ID 类型
    PIDTYPE_PGID, // 进程组 ID 类型
    PIDTYPE_MAX   // 最大的 PID 类型索引编号 + 1
};

enum procstate { UNUSED,
                 USED,
                 SLEEPING,
                 RUNNABLE,
                 RUNNING,
                 ZOMBIE,
                 STATEMAX };

// Per-process state
struct proc {
    struct spinlock lock;

    // p->lock must be held when using these:
    enum procstate state; // Process state
    void *chan;           // If non-zero, sleeping on chan
    int killed;           // If non-zero, have been killed
    struct list_head head_vma;
    int exit_state; // Exit status to be returned to parent's wait
    pid_t pid;      // Process ID

    // maybe also need thread lock to access, p->tlock must be held
    struct spinlock tlock;

    // these are private to the process, so p->lock need not be held.
    uint64 kstack;                // Virtual address of kernel stack
    uint64 sz;                    // Size of process memory (bytes)
    pagetable_t pagetable;        // User page table
    struct trapframe *trapframe;  // data page for trampoline.S
    struct context context;       // swtch() here to run process
    struct _file *_ofile[NOFILE]; // Open files
    struct _inode *_cwd;          // Current directory
    // struct file *ofile[NOFILE];  // Open files(only in xv6)
    // struct inode *cwd;           // Current directory(only in xv6)
    char name[16]; // Process name (debugging)

    // wait_lock must be held when using this:
    struct proc *parent; // Parent process

    struct list_head state_list;   // its state queue
    struct proc *first_child;      // its first child!!!!!!!
    struct list_head sibling_list; // its sibling

    int sigpending;                   // have signal?
    struct signal_struct *sig;        // signal
    sigset_t blocked;                 // the blocked signal
    struct sigpending pending;        // pending (private)
    struct sigpending shared_pending; // pending (shared)

    tgid_t tgid;                   // thread group id
    int thread_cnt;                // the count of threads
    struct list_head thread_group; // thread group
    struct proc *group_leader;     // its proc thread group leader

    pgrp_t pgid; // proc group id

    struct list_head wait_list; // waiting  queue
    pid_t *ctid;
    // struct spinlock wait_lock;
    struct semaphore sem_wait_chan_parent;
    struct semaphore sem_wait_chan_self;

    long tms_stime;   // system mode time(ticks)
    long tms_utime;   // user mode time(ticks)
    long create_time; // create time(ticks)
    long enter_time;  // enter kernel time(ticks)
};

// 1. struct proc and pid
void procinit(void);
struct proc *current();
int allocpid();
void initret(void);
void forkret(void);
struct proc *allocproc(void);
void freeproc(struct proc *p);
struct proc *find_get_pid(pid_t);
void deleteChild(struct proc *parent, struct proc *child);
void appendChild(struct proc *parent, struct proc *child);
void procChildrenChain(struct proc *p);

// 2. the lifetime of proc
int do_clone(int, uint64, pid_t, uint64, pid_t *);
void do_exit(int);
int waitpid(pid_t, uint64, int);

void reparent(struct proc *p);
// 3. debug
void procdump(void);

#endif