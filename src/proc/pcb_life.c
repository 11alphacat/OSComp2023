#include "common.h"
#include "param.h"
#include "memory/memlayout.h"
#include "riscv.h"
#include "atomic/spinlock.h"
#include "proc/pcb_life.h"
#include "memory/allocator.h"
#include "kernel/trap.h"
#include "kernel/cpu.h"
#include "memory/vm.h"
#include "fs/inode/fs.h"
#include "fs/inode/file.h"
#include "debug.h"
#include "atomic/atomic.h"
#include "proc/sched.h"
#include "proc/pcb_life.h"
#include "proc/pcb_mm.h"
#include "proc/exec.h"
#include "proc/signal.h"
#include "proc/wait_queue.h"
#include "memory/vma.h"

struct proc proc[NPROC];
struct proc *initproc;

atomic_t nextpid;
// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

extern void fsinit(int);

extern PCB_Q_t unused_q, used_q, running_q, runnable_q, sleeping_q, zombie_q;

// Return the current struct proc *, or zero if none.
struct proc * myproc(void) {
    push_off();
    struct cpu *c = mycpu();
    struct proc *p = c->proc;
    pop_off();
    return p;
}

int allocpid() {
    int pid  = atomic_inc_return(&nextpid);
    return pid;
}

// Set up first user process.
void userinit(void) {
    struct proc *p;

    p = allocproc();
    initproc = p;

    p->sz = 0;

    p->cwd = namei("/");

    p->state = RUNNABLE;

    release(&p->lock);
}


// initialize the proc table.
void procinit(void) {
    struct proc *p;

    atomic_set(&nextpid, 1);
    initlock(&wait_lock, "wait_lock");
    
    PCB_Q_ALL_INIT();
    for (p = proc; p < &proc[NPROC]; p++) {
        initlock(&p->lock, "proc");
        p->state = UNUSED;
        p->kstack = KSTACK((int)(p - proc));
        PCB_Q_push_back(&unused_q, p);
    }
    return ;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
struct proc *
allocproc(void) {
    struct proc *p;

    for (p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if (p->state == UNUSED) {
            goto found;
        } else {
            release(&p->lock);
        }
    }
    return 0;

found:
    p->pid = allocpid();
    p->state = USED;

    // Allocate a trapframe page.
    if ((p->trapframe = (struct trapframe *)kalloc()) == 0) {
        freeproc(p);
        release(&p->lock);
        return 0;
    }

    // An empty user page table.
    p->pagetable = proc_pagetable(p);
    if (p->pagetable == 0) {
        freeproc(p);
        release(&p->lock);
        return 0;
    }

    // Set up new context to start executing at forkret,
    // which returns to user space.
    memset(&p->context, 0, sizeof(p->context));
    p->context.ra = (uint64)forkret;
    p->context.sp = p->kstack + PGSIZE;

    INIT_LIST_HEAD(&p->head_vma);
    return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
void
freeproc(struct proc *p) {
    if (p->trapframe)
        kfree((void *)p->trapframe);
    p->trapframe = 0;
    if (p->pagetable)
        proc_freepagetable(p->pagetable, p->sz);
    p->pagetable = 0;
    p->sz = 0;
    p->pid = 0;
    p->parent = 0;
    p->name[0] = 0;
    p->chan = 0;
    p->killed = 0;
    p->xstate = 0;
    p->state = UNUSED;
}


// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int fork(void) {
    int i, pid;
    struct proc *np;
    struct proc *p = myproc();

    // Allocate process.
    if ((np = allocproc()) == 0) {
        return -1;
    }

    /* Copy vma */
    if (vmacopy(np) < 0) {
        freeproc(np);
        release(&np->lock);
        return -1;
    }

    // Copy user memory from parent to child.
    if (uvmcopy(p->pagetable, np->pagetable, p->sz) < 0) {
        freeproc(np);
        release(&np->lock);
        return -1;
    }
    np->sz = p->sz;

    // copy saved user registers.
    *(np->trapframe) = *(p->trapframe);

    // Cause fork to return 0 in the child.
    np->trapframe->a0 = 0;

    // increment reference counts on open file descriptors.
    for (i = 0; i < NOFILE; i++)
        if (p->ofile[i])
            np->ofile[i] = filedup(p->ofile[i]);
    np->cwd = idup(p->cwd);

    safestrcpy(np->name, p->name, sizeof(p->name));

    pid = np->pid;

    release(&np->lock);

    // acquire(&wait_lock);
    np->parent = p;
    // release(&wait_lock);

    acquire(&np->lock);
    np->state = RUNNABLE;
    release(&np->lock);

    return pid;
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void forkret(void) {
    static int first = 1;

    // Still holding p->lock from scheduler.
    release(&myproc()->lock);

    if (first) {
        // File system initialization must be run in the context of a
        // regular process (e.g., because it calls sleep), and thus cannot
        // be run from main().
        first = 0;
        fsinit(ROOTDEV);
        char *argv[] = {"init", 0};
        myproc()->trapframe->a0 = exec("/init", argv);
    }

    usertrapret();
}


int clone(int flags, uint64 stack , pid_t ptid, uint64 tls, pid_t* ctid)
{
    return 1;
}

int do_fork()
{
    return 1;
}


// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void exit(int status) {
    struct proc *p = myproc();

    if (p == initproc)
        panic("init exiting");

    vmafree(p);

    // Close all open files.
    for (int fd = 0; fd < NOFILE; fd++) {
        if (p->ofile[fd]) {
            struct file *f = p->ofile[fd];
            fileclose(f);
            p->ofile[fd] = 0;
        }
    }

    begin_op();
    iput(p->cwd);
    end_op();
    p->cwd = 0;

    acquire(&wait_lock);

    // Give any children to init.
    reparent(p);

    // Parent might be sleeping in wait().
    wakeup(p->parent);

    acquire(&p->lock);

    p->xstate = status;
    p->state = ZOMBIE;

    release(&wait_lock);

    // Jump into the scheduler, never to return.
    sched();
    panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(uint64 addr) {
    struct proc *pp;
    int havekids, pid;
    struct proc *p = myproc();

    acquire(&wait_lock);

    for (;;) {
        // Scan through table looking for exited children.
        havekids = 0;
        for (pp = proc; pp < &proc[NPROC]; pp++) {
            if (pp->parent == p) {
                // make sure the child isn't still in exit() or swtch().
                acquire(&pp->lock);

                havekids = 1;
                if (pp->state == ZOMBIE) {
                    // Found one.
                    pid = pp->pid;
                    if (addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate, sizeof(pp->xstate)) < 0) {
                        release(&pp->lock);
                        release(&wait_lock);
                        return -1;
                    }
                    freeproc(pp);
                    release(&pp->lock);
                    release(&wait_lock);
                    return pid;
                }
                release(&pp->lock);
            }
        }

        // No point waiting if we don't have any children.
        if (!havekids || killed(p)) {
            release(&wait_lock);
            return -1;
        }

        // Wait for a child to exit.
        sleep(p, &wait_lock); //DOC: wait-sleep
    }
}

int wait4(pid_t pid, int *status, int options) {
    
    int ret=0;
    return ret;
}

int do_wait()
{
    return 1;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void reparent(struct proc *p) {
    struct proc *pp;

    for (pp = proc; pp < &proc[NPROC]; pp++) {
        if (pp->parent == p) {
            pp->parent = initproc;
            wakeup(initproc);
        }
    }
}



// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump(void) {
    static char *states[] = {
        [UNUSED] "unused",
        [USED] "used",
        [SLEEPING] "sleep ",
        [RUNNABLE] "runble",
        [RUNNING] "run   ",
        [ZOMBIE] "zombie"};
    struct proc *p;
    char *state;

    printf("\n");
    for (p = proc; p < &proc[NPROC]; p++) {
        if (p->state == UNUSED)
            continue;
        if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
            state = states[p->state];
        else
            state = "???";
        printf("%d %s %s", p->pid, state, p->name);
        printf("\n");
    }
}
