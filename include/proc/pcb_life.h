#ifndef __PCB_LIFE_H__
#define __PCB_LIFE_H__

#include "common.h"
#include "param.h"
#include "atomic/spinlock.h"
#include "kernel/kthread.h"
#include "lib/list.h"
#include "proc/signal.h"
#include "atomic/semaphore.h"
// #include "fs/fat/fat32_disk.h"

#define NPROC 32 // maximum number of processes
#define INIT_PID 1
#define SHELL_PID 2

struct file;
struct inode;

enum procstate { PCB_UNUSED,
                 PCB_USED,
                 PCB_ZOMBIE,
                 PCB_STATEMAX };

// Per-process state
struct proc {
    // the spinlock protecting proc
    struct spinlock lock;

    // p->lock must be held when using these:
    pid_t pid;            // Process ID
    enum procstate state; // Process state
    int killed;           // If non-zero, have been killed
    int exit_state;       // Exit status to be returned to parent's wait
    struct list_head head_vma;

    // these are private to the process, so p->lock need not be held.
    uint64 sz;                   // Size of proc memory (bytes)
    pagetable_t pagetable;       // User page table
    struct file *_ofile[NOFILE]; // Open files
    struct inode *_cwd;          // Current directory
    char name[16];               // Process name (debugging)

    // proc state queue
    struct list_head state_list; // its state queue

    // proc children and siblings
    struct proc *parent;           // Parent process
    struct proc *first_child;      // its first child!!!! (one direction)
    struct list_head sibling_list; // its sibling

    // thread group
    struct thread_group *tg;
    pid_t ctid;

    struct spinlock tlock;
    // // signal
    // int sig_pending_cnt;                   // have signal?
    // struct sighand *sig;        // signal
    // sigset_t blocked;                 // the blocked signal
    // struct sigpending pending;        // pending (private)

    // system mode time and user mode time
    // long tms_stime;   // system mode time(ticks)
    // long tms_utime;   // user mode time(ticks)
    // long create_time; // create time(ticks)
    // long enter_time;  // enter kernel time(ticks)
};

void deleteChild(struct proc *parent, struct proc *child);
void appendChild(struct proc *parent, struct proc *child);
void procChildrenChain(struct proc *p);
struct proc *proc_current(void);
struct proc *alloc_proc(void);
struct proc *create_proc();
void free_proc(struct proc *p);
void init_ret(void);
void proc_init(void);
void proc_setkilled(struct proc *p);
int proc_killed(struct proc *p);
struct proc *find_get_pid(pid_t pid);
int do_clone(int flags, uint64 stack, pid_t ptid, uint64 tls, pid_t *ctid);
void do_exit(int status);
int waitpid(pid_t pid, uint64 status, int options);
void reparent(struct proc *p);
void proc_thread_print(void);
uint8 get_current_procs();

#endif