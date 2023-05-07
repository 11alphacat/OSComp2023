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
    struct semaphore sem_wait_chan;
};

// per-process data for the trap handling code in trampoline.S.
// sits in a page by itself just under the trampoline page in the
// user page table. not specially mapped in the kernel page table.
// uservec in trampoline.S saves user registers in the trapframe,
// then initializes registers from the trapframe's
// kernel_sp, kernel_hartid, kernel_satp, and jumps to kernel_trap.
// usertrapret() and userret in trampoline.S set up
// the trapframe's kernel_*, restore user registers from the
// trapframe, switch to the user page table, and enter user space.
// the trapframe includes callee-saved user registers like s0-s11 because the
// return-to-user path via usertrapret() doesn't return through
// the entire kernel call stack.
struct trapframe {
    /*   0 */ uint64 kernel_satp;   // kernel page table
    /*   8 */ uint64 kernel_sp;     // top of process's kernel stack
    /*  16 */ uint64 kernel_trap;   // usertrap()
    /*  24 */ uint64 epc;           // saved user program counter
    /*  32 */ uint64 kernel_hartid; // saved kernel tp
    /*  40 */ uint64 ra;
    /*  48 */ uint64 sp;
    /*  56 */ uint64 gp;
    /*  64 */ uint64 tp;
    /*  72 */ uint64 t0;
    /*  80 */ uint64 t1;
    /*  88 */ uint64 t2;
    /*  96 */ uint64 s0;
    /* 104 */ uint64 s1;
    /* 112 */ uint64 a0;
    /* 120 */ uint64 a1;
    /* 128 */ uint64 a2;
    /* 136 */ uint64 a3;
    /* 144 */ uint64 a4;
    /* 152 */ uint64 a5;
    /* 160 */ uint64 a6;
    /* 168 */ uint64 a7;
    /* 176 */ uint64 s2;
    /* 184 */ uint64 s3;
    /* 192 */ uint64 s4;
    /* 200 */ uint64 s5;
    /* 208 */ uint64 s6;
    /* 216 */ uint64 s7;
    /* 224 */ uint64 s8;
    /* 232 */ uint64 s9;
    /* 240 */ uint64 s10;
    /* 248 */ uint64 s11;
    /* 256 */ uint64 t3;
    /* 264 */ uint64 t4;
    /* 272 */ uint64 t5;
    /* 280 */ uint64 t6;
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
int do_fork(void);
int do_clone(int, uint64, pid_t, uint64, pid_t *);
void do_exit(int);
int do_wait(uint64);
int waitpid(pid_t, uint64, int);

void reparent(struct proc *p);
void wakeup_proc(struct proc *p);
// 3. debug
void procdump(void);

#endif