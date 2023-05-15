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
#include "fs/stat.h"
#include "fs/vfs/fs.h"
#include "fs/fat/fat32_file.h"

int cnt_exit = 0;
int cnt_wakeup = 0;
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
    // if (p == NULL) {
    //     Log("here!");
    // }
    pop_off();
    return p;
}

int allocpid() {
    return atomic_inc_return(&nextpid);
}

struct _file console;
void _userinit(void) {
    struct proc *p;

    p = allocproc();
    initproc = p;
    safestrcpy(p->name, "/init", 10);
    p->sz = 0;
    p->state = RUNNABLE;

    console.f_type = T_DEVICE;
    console.f_mode = O_RDWR;
    console.f_major = CONSOLE;

    PCB_Q_changeState(p, RUNNABLE);
    // p->__ofile[0] = &console;
    release(&p->lock);

    return;
}

void initret(void) {
    extern struct _superblock fat32_sb;
    fat32_fs_mount(ROOTDEV, &fat32_sb);
    current()->_cwd = fat32_name_inode("/");
    // console.f_tp.f_inode = fat32_inode_create("console.dev",T_DEVICE);
    // struct _inode* ip = fat32_inode_create("console.dev",T_DEVICE);
    // ASSERT(ip!=NULL);
    // console.f_tp.f_inode = ip;
    char *argv[] = {"init", 0};
    current()->trapframe->a0 = do_execve("/init", argv, NULL);
    // fsinit(ROOTDEV);
    // char *argv[] = {"init", 0};
    // current()->trapframe->a0 = do_execve("/init", argv, NULL);
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
        // initlock(&p->wait_lock, "proc_wait_lock");
        sema_init(&p->sem_wait_chan_self, 0, "proc_sema_chan_self");
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

    p = PCB_Q_provide(&unused_q, 1);
    if (p == NULL)
        return 0;

    acquire(&p->lock);
    p->pid = allocpid();

    PCB_Q_changeState(p, USED);
    p->first_child = NULL;
    INIT_LIST_HEAD(&p->sibling_list);
    sema_init(&p->sem_wait_chan_parent, 0, "proc_sema_chan_parent");

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

#include "debug.h"
#define nochildren(p) (p->first_child == NULL)
#define nosibling(p) (list_empty(&p->sibling_list))
#define firstchild(p) (p->first_child)
#define nextsibling(p) (list_first_entry(&(p->sibling_list), struct proc, sibling_list))

void deleteChild(struct proc *parent, struct proc *child) {
    if (nosibling(child)) {
        parent->first_child = NULL;
    } else {
        struct proc *firstchild = firstchild(parent);
        if (child == firstchild) {
            parent->first_child = nextsibling(firstchild);
        }
        list_del_reinit(&child->sibling_list);
    }
}

void appendChild(struct proc *parent, struct proc *child) {
    if (nochildren(parent)) {
        parent->first_child = child;
    } else {
        list_add_tail(&child->sibling_list, &(firstchild(parent)->sibling_list));
    }
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void forkret(void) {
    // Still holding p->lock from scheduler.
    release(&current()->lock);
    if (current() == initproc) {
        initret();
    }
    usertrapret();
}

int do_clone(int flags, uint64 stack, pid_t ptid, uint64 tls, pid_t *ctid) {
    int pid;
    struct proc *np;
    struct proc *p = current();

    // Allocate process.
    if ((np = allocproc()) == 0) {
        return -1;
    }
    // copy saved user registers.
    *(np->trapframe) = *(p->trapframe);
    // Cause fork to return 0 in the child.
    np->trapframe->a0 = 0;

    /* Copy vma */
    if (vmacopy(np) < 0) {
        freeproc(np);
        release(&np->lock);
        return -1;
    }

    // Copy user memory from parent to child.
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
    np->sz = p->sz;
    // increment reference counts on open file descriptors.
    // TODO : fdtable
    if (flags & CLONE_FILES) {
        // for (int i = 0; i < NOFILE; i++)
        //     if (p->_ofile[i])
        //         np->_ofile[i] = p->_ofile[i];
    } else {
        for (int i = 0; i < NOFILE; i++)
            if (p->_ofile[i])
                np->_ofile[i] = fat32_filedup(p->_ofile[i]);
        // TODO : clone a completely same fdtable
    }
    // TODO : vfs inode cmd
    np->_cwd = fat32_inode_dup(p->_cwd);

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
        // np->trapframe->tp = tls;
    }
    // 子线程中存储子线程 ID 的变量指针
    if (flags & CLONE_CHILD_SETTID) {
        np->ctid = ctid;
    }
    if (stack) {
        np->trapframe->sp = stack;
    }
    safestrcpy(np->name, p->name, sizeof(p->name));

    pid = np->pid;
    np->parent = p;
    acquire(&p->lock);
    appendChild(p, np);
    release(&p->lock);

#ifdef __DEBUG_PROC__
    printfRed("clone : %d -> %d\n", p->pid, np->pid); // debug
#endif

    PCB_Q_changeState(np, RUNNABLE);
    release(&np->lock);
    return pid;
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
        if (p->_ofile[fd]) {
            struct _file *f = p->_ofile[fd];
            fat32_fileclose(f);
            p->_ofile[fd] = 0;
        }
    }
    fat32_inode_put(p->_cwd);
    p->_cwd = 0;

    // Give any children to init.
    reparent(p);

    acquire(&p->lock);
    p->exit_state = status << 8;

    PCB_Q_changeState(p, ZOMBIE);
    sema_signal(&p->parent->sem_wait_chan_parent);
    sema_signal(&p->sem_wait_chan_self);

#ifdef __DEBUG_PROC__
    printfGreen("exit : %d has exited\n", p->pid);                // debug
    printfGreen("exit : %d wakeup %d\n", p->pid, p->parent->pid); // debug
#endif
    // Jump into the scheduler, never to return.
    sched();
    panic("zombie exit");
}

int waitpid(pid_t pid, uint64 status, int options) {
    struct proc *p = current();
    if (pid < -1)
        pid = -pid;

    ASSERT(pid != 0);

    if (nochildren(p)) {
#ifdef __DEBUG_PROC__
        printf("wait : %d hasn't children\n", p->pid); // debug
#endif
        return -1;
    }

    if (killed(p)) {
#ifdef __DEBUG_PROC__
        printf("wait : %d has been killed\n", p->pid); // debug
#endif
        return -1;
    }
    while (1) {
        sema_wait(&p->sem_wait_chan_parent);
#ifdef __DEBUG_PROC__
        printfBlue("wait : %d wakeup\n", p->pid); // debug
#endif
        struct proc *p_child = NULL;
        struct proc *p_tmp = NULL;
        struct proc *p_first = firstchild(p);
        int flag = 1;
        list_for_each_entry_safe_given_first(p_child, p_tmp, p_first, sibling_list, flag) {
            // shell won't exit !!!
            if (pid > 0 && p_child->pid == pid) {
                sema_wait(&p_child->sem_wait_chan_self);
#ifdef __DEBUG_PROC__
                printfBlue("wait : %d wakeup self\n", p->pid); // debug
#endif
            }

            acquire(&p_child->lock);
            if (p_child->state == ZOMBIE) {
                // if(p==initproc)
                //     printfRed("唤醒,pid : %d, %d\n",p_child->pid, ++cnt_wakeup); // debug

                // ASSERT(p_child->pid!=SHELL_PID);
                pid = p_child->pid;
                if (status != 0 && copyout(p->pagetable, status, (char *)&(p_child->exit_state), sizeof(p_child->exit_state)) < 0) {
                    release(&p_child->lock);
                    return -1;
                }
                freeproc(p_child);

                acquire(&p->lock);
                deleteChild(p, p_child);
                release(&p->lock);

                release(&p_child->lock);

#ifdef __DEBUG_PROC__
                printfBlue("wait : %d delete %d\n", p->pid, pid); // debug
#endif
                return pid;
            }
            release(&p_child->lock);
        }
        printf("%d\n", p->pid);
        printf("%d\n", p->sem_wait_chan_parent.value);
        // if(p==initproc) {
        //     procChildrenChain(initproc);
        // }
        panic("waitpid : invalid wakeup for semaphore!");
    }
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void reparent(struct proc *p) {
    struct proc *p_child = NULL;
#ifdef __DEBUG_PROC__
    printf("reparent : %d is going to exit and reparent its children\n", p->pid); // debug
#endif
    if (!nochildren(p)) {
        struct proc *p_first_c = firstchild(p);
        struct proc *p_tmp = NULL;
        int flag = 1;

        list_for_each_entry_safe_given_first(p_child, p_tmp, p_first_c, sibling_list, flag) {
            acquire(&p_child->lock);

            acquire(&p->lock);
            deleteChild(p, p_child);
            release(&p->lock);
            // maybe the lock of initproc can be removed

            acquire(&initproc->lock);
            appendChild(initproc, p_child);
            release(&initproc->lock);

            p_child->parent = initproc;
            if (p_child->state == ZOMBIE) {
                sema_signal(&initproc->sem_wait_chan_parent);
                // printf("退出,pid : %d, %d\n",p_child->pid, ++cnt_exit);// debug
#ifdef __DEBUG_PROC__
                printfBWhite("reparent : zombie %d has exited\n", p_child->pid); // debug
                printfBWhite("reparent : zombie %d wakeup 1\n", p_child->pid);   // debug
#endif
            }
            release(&p_child->lock);

#ifdef __DEBUG_PROC__
            printf("reparent : %d reparent %d -> 1\n", p->pid, p_child->pid); // debug
#endif
            if (p->first_child == NULL) {
                break; // !!!!
            }
        }
        // procChildrenChain(initproc);
        ASSERT(nochildren(p));
    } else {
#ifdef __DEBUG_PROC__
        printf("reparent : %d has no children\n", p->pid); // debug
#endif
    }
}

// print children of proc p
void procChildrenChain(struct proc *p) {
    char tmp_str[1000];
    int len = 0;
    len += sprintf(tmp_str, "=======debug======\n");
    len += sprintf(tmp_str + len, "proc : %d\n", p->pid, p->name);
    struct proc *p_pos = NULL;
    struct proc *p_first = firstchild(p);
    if (p_first == NULL) {
        len += sprintf(tmp_str + len, "no children!!!\n");
    } else {
        len += sprintf(tmp_str + len, "%d", p_first->pid, p_first->name);
        list_for_each_entry(p_pos, &p_first->sibling_list, sibling_list) {
            len += sprintf(tmp_str + len, "->%d", p_pos->pid, p_pos->name);
        }
    }
    printf("%s\n", tmp_str);
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
