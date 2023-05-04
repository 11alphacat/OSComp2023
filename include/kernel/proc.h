#ifndef __PROC_H__
#define __PROC_H__

#include "common.h"
#include "param.h"
#include "atomic/spinlock.h"
#include "kernel/kthread.h"
#include "list.h"

#define NPROC 64 // maximum number of processes

struct file;
struct inode;

enum pid_type {
    PIDTYPE_PID,  // 进程 ID 类型
    PIDTYPE_PGID, // 进程组 ID 类型
    PIDTYPE_SID,  // 会话 ID 类型
    PIDTYPE_MAX   // 最大的 PID 类型索引编号 + 1
};

enum procstate { UNUSED,
                 USED,
                 SLEEPING,
                 RUNNABLE,
                 RUNNING,
                 ZOMBIE,
                 STATEMAX };

typedef int pid_t;

// Per-process state
struct proc {
    struct spinlock lock;

    // p->lock must be held when using these:
    enum procstate state; // Process state
    void *chan;           // If non-zero, sleeping on chan
    int killed;           // If non-zero, have been killed
    int xstate;           // Exit status to be returned to parent's wait
    int pid;              // Process ID

    struct list_head head_vma;
    // wait_lock must be held when using this:
    struct proc *parent; // Parent process

    // these are private to the process, so p->lock need not be held.
    uint64 kstack;               // Virtual address of kernel stack
    uint64 sz;                   // Size of process memory (bytes)
    pagetable_t pagetable;       // User page table
    struct trapframe *trapframe; // data page for trampoline.S
    struct context context;      // swtch() here to run process
    struct file *ofile[NOFILE];  // Open files
    struct inode *cwd;           // Current directory
    char name[16];               // Process name (debugging)

    struct list_head PCB_list;
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

// 1. get current proc
struct proc *current();

// 2. allocate a new pid
int allocpid();

void procinit(void);
pagetable_t proc_pagetable(struct proc *);
void proc_mapstacks(pagetable_t);
void proc_freepagetable(pagetable_t, uint64);
int growproc(int);

int fork(void);
int clone();
int do_fork();

void exit(int);

int kill(int);
void setkilled(struct proc *);
int killed(struct proc *);

int exec(char *, char **);
int execve();
int do_execve();

void yield(void);
void sched(void);
void scheduler(void) __attribute__((noreturn));

int wait(uint64);
int wait4(pid_t, int *, int);
int do_wait();

void sleep(void *, struct spinlock *);
void wakeup(void *);

int either_copyout(int user_dst, uint64 dst, void *src, uint64 len);
int either_copyin(void *dst, int user_src, uint64 src, uint64 len);

void procdump(void);

#endif // __PROC_H__