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
#include "proc/cond.h"
#include "test.h"
#include "proc/options.h"

struct proc proc[NPROC];
struct proc *initproc;

atomic_t nextpid;
// when using p->parent, we must acquire wait_lock
// avoid wakeup lost!!!
struct spinlock wait_lock;

extern void fsinit(int);

extern PCB_Q_t unused_q, used_q, runnable_q, sleeping_q, zombie_q;
extern PCB_Q_t *STATES[STATEMAX];

char proc_lock_name[NPROC][10];

// Return the current struct proc *, or zero if none.
struct proc *current(void) {
    push_off();
    struct cpu *c = mycpu();
    struct proc *p = c->proc;
    pop_off();
    return p;
}

int allocpid() {
    int pid = atomic_inc_return(&nextpid);
    return pid;
}

// Set up first user process.
void userinit(void) {
    struct proc *p;

    p = allocproc();
    initproc = p;

    p->sz = 0;
    p->cwd = namei("/");
    // p->cwd = fat32_inode_name("/");

    PCB_Q_changeState(p, RUNNABLE);
    p->state = RUNNABLE;

    release(&p->lock);
}

void initret(void) {
    fsinit(ROOTDEV);
    char *argv[] = {"init", 0};
    current()->trapframe->a0 = do_execve("/init", argv, NULL);
}

// initialize the proc table.
void procinit(void) {
    struct proc *p;

    atomic_set(&nextpid, 1);
    initlock(&wait_lock, "wait_lock");

    PCB_Q_ALL_INIT();

    // for (p = proc; p < &proc[NPROC]; p++) {
    for (int i = 0; i < NPROC; i++) {
        p = proc + i;
        sprintf(proc_lock_name[i], "proc_%d", i);
        initlock(&p->lock, proc_lock_name[i]);
        initlock(&p->wait_lock, "proc_wait_lock");
        p->state = UNUSED;
        p->kstack = KSTACK((int)(p - proc));
        PCB_Q_push_back(&unused_q, p);  
    }
    return;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
struct proc *
allocproc(void) {
    struct proc *p;

    p = PCB_Q_provide(&unused_q);
    if (p == NULL)
        return 0;

    acquire(&p->lock);
    p->pid = allocpid();

    PCB_Q_changeState(p, USED);
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

    // don't realse the lock of process p
    return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
void freeproc(struct proc *p) {
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
    p->exit_state = 0;

    PCB_Q_changeState(p, UNUSED);
    p->state = UNUSED;
}

// find the proc we search
// return the proc pointer with spinlock
struct proc *find_get_pid(pid_t pid) {
    struct proc *p;
    for (p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if (p->pid == pid) {
            return p;
        }
        release(&p->lock);
    }
    return NULL;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int do_fork(void) {
    int i, pid;
    struct proc *np;
    struct proc *p = current();

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
    PCB_Q_changeState(np, RUNNABLE);
    np->state = RUNNABLE;

    release(&np->lock);

    return pid;
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void forkret(void) {
    // Still holding p->lock from scheduler.
    release(&current()->lock);
    if(current()==initproc){
        initret();
    }
    usertrapret();
}

int do_clone(int flags, uint64 stack, pid_t ptid, uint64 tls, pid_t *ctid) {
    struct proc *np;
    // Allocate process.
    if ((np = allocproc()) == 0) {
        return -1;
    }
    // uint64* long_p = &stack;
    // uint64 fn = long_p[0];
    // uint64 arg = long_p[1];
    struct proc *p = current();

    // copy the trapframe
    *(np->trapframe) = *(p->trapframe);

    // TODO : mmap
    if (flags & CLONE_VM) {
        np->pagetable = p->pagetable;
    } else {
        if (uvmcopy(p->pagetable, np->pagetable, p->sz) < 0) {
            freeproc(np);
            release(&np->lock);
            return -1;
        }
    }
    // TODO : fdtable
    if (flags & CLONE_FILES) {
        for (int i = 0; i < NOFILE; i++)
            if (p->ofile[i])
                np->ofile[i] = filedup(p->ofile[i]);
    } else {
        // TODO : clone a completely same fdtable
    }

    // TODO : vfs inode cmd
    np->cwd = idup(p->cwd);
    // TODO : signal
    if (flags & CLONE_SIGHAND) {
        np->sig = p->sig;
    } else {
        // TODO : create a new signal
    }

    // TODO : mount point CLONE_FS

    // store the parent pid
    if (flags & CLONE_PARENT_SETTID) {
        if (either_copyin(&ptid, 1, p->pid, sizeof(pid_t)) == -1)
            return -1;
    }
    // set the tls (Thread-local Storage，TLS)
    // RISC-V使用TP寄存器
    if (flags & CLONE_SETTLS) {
        np->trapframe->tp = tls;
    }
    // 子线程中存储子线程 ID 的变量指针
    if (flags & CLONE_CHILD_SETTID) {
        np->ctid = ctid;
    }

    // copy the name of proc
    safestrcpy(np->name, p->name, sizeof(p->name));

    // avoid parent miss this new child
    acquire(&wait_lock);
    np->parent = p;
    release(&wait_lock);

    // same as fork, its child return 0
    np->trapframe->a0 = 0;

    // change its state tp RUNNABLE
    // acquire(&np->lock);
    PCB_Q_changeState(np, RUNNABLE);
    np->state = RUNNABLE;

    release(&np->lock);

    // its parent return child's pid
    return np->pid;
}


// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void do_exit(int status) {
    struct proc *p = current();

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
    // TODO: fat32 file system

    begin_op();
    iput(p->cwd);
    // fat32_inode_put(p->cwd);
    end_op();
    p->cwd = 0;

    acquire(&wait_lock);

    // Give any children to init.
    reparent(p);
    // Parent might be sleeping in wait().
    wakeup(p->parent);

    acquire(&p->lock);
    p->exit_state = status;
    PCB_Q_changeState(p, ZOMBIE);
    p->state = ZOMBIE;

    release(&wait_lock);

    // Jump into the scheduler, never to return.
    sched();
    panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int do_wait(uint64 addr) {
    struct proc *pp;
    int havekids, pid;
    struct proc *p = current();

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
                    if (addr != 0 && copyout(p->pagetable, addr, (char *)&pp->exit_state, sizeof(pp->exit_state)) < 0) {
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
        sleep(p, &wait_lock); // DOC: wait-sleep
    }
}
int waitpid(pid_t pid, uint64 status, int options) {
    struct proc *p = current();
    int havekids;
    int havetarget;
    if(pid<-1){
        pid = -pid;
    }
    acquire(&wait_lock);
    for (;;) {  
        havekids = 0;
        havetarget = 0;
        for (struct proc* p_child = proc; p_child < &proc[NPROC]; p_child++) {
            if (p_child->parent == p) {
                havekids = 1;  
                // make sure the child isn't still in exit() or swtch().
                acquire(&p_child->lock);

                // TODO : thread

                // TODO : proc group (进程组) pid = 0 

                if(pid!=-1 && p_child->pid!= pid) {
                    release(&p_child->lock);
                    continue;
                }
                havetarget = 1;
                if (p_child->state == ZOMBIE) {
                    // Found one.
                    pid = p_child->pid;
                    if (status != 0 && copyout(p->pagetable, status, (char *)&p_child->exit_state, sizeof(p_child->exit_state)) < 0) {
                        release(&p_child->lock);
                        release(&wait_lock);
                        return -1;
                    }
                    freeproc(p_child);
                    release(&p_child->lock);
                    release(&wait_lock);
                    return pid;
                }
                release(&p_child->lock);
            }
        }
        if ((!havekids&&(options&WNOHANG)) || killed(p) || !havetarget) {
            release(&wait_lock);
            return -1;
        }
        sleep(p, &wait_lock);
    }
}


// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void reparent(struct proc *p) {
    struct proc *pp;
    for (pp = proc; pp < &proc[NPROC]; pp++) {
        if (pp->parent == p) {
            pp->parent = initproc;
            // wakeup(initproc);
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
