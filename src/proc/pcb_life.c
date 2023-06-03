#include "common.h"
#include "param.h"
#include "riscv.h"
#include "debug.h"
#include "test.h"
#include "atomic/atomic.h"
#include "atomic/spinlock.h"
#include "memory/memlayout.h"
#include "memory/allocator.h"
#include "memory/vm.h"
#include "memory/vma.h"
#include "kernel/trap.h"
#include "kernel/cpu.h"
#include "proc/pcb_life.h"
#include "proc/cond.h"
#include "proc/sched.h"
#include "proc/pcb_life.h"
#include "proc/proc_mm.h"
#include "proc/exec.h"
#include "proc/signal.h"
#include "proc/wait_queue.h"
#include "proc/options.h"
#include "fs/stat.h"
#include "fs/vfs/fs.h"
#include "fs/vfs/ops.h"
#include "fs/fat/fat32_file.h"

struct proc proc[NPROC];
struct proc *initproc;

extern PCB_Q_t unused_p_q, used_p_q, zombie_p_q;
extern TCB_Q_t unused_t_q, runnable_t_q, sleeping_t_q;

extern PCB_Q_t *STATES[PCB_STATEMAX];

char proc_lock_name[NPROC][10];
atomic_t next_pid;
atomic_t count_pid;

#define alloc_pid (atomic_inc_return(&next_pid))
#define cnt_pid_inc (atomic_inc_return(&count_pid))
#define cnt_pid_dec (atomic_dec_return(&count_pid))

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

// Return the current struct tcb *, or zero if none.
struct proc *proc_current(void) {
    return thread_current()->p;
}

// allocate a new proc (return with lock)
struct proc *alloc_proc(void) {
    struct proc *p;
    // fetch a unused proc from unused queue
    // fetch a unused thread from unused queue as its group leader
    p = PCB_Q_provide(&unused_p_q, 1);
    if (p == NULL)
        return 0;

    acquire(&p->lock); // return with lock

    p->pid = alloc_pid;
    cnt_pid_inc;

    // state
    PCB_Q_changeState(p, PCB_USED);

    // proc family
    p->first_child = NULL;
    INIT_LIST_HEAD(&p->sibling_list);

    // slob!!!
    p->mm = alloc_mm();
    if (p->mm == NULL) {
        Warn("fix error handler!");
        free_proc(p);
        release(&p->lock);
        return 0;
    }

    // thread group (list head) sets to NULL
    if ((p->tg = (struct thread_group *)kalloc()) == 0) {
        free_proc(p);
        release(&p->lock);
        return 0;
    }

    tginit(p->tg);

    return p;
}

// create a proc with a group leader thread
struct proc *create_proc() {
    struct tcb *t = NULL;
    struct proc *p = NULL;

    if ((p = alloc_proc()) == 0) {
        return 0;
    }

    if ((t = alloc_thread()) == 0) {
        free_proc(p);
        return 0;
    }

    proc_join_thread(p, t);

    // wait semaphore
    sema_init(&t->sem_wait_chan_parent, 0, "thread_sema_chan");

    // uvm_thread_trapframe(p->mm->pagetable, 0, (paddr_t)t->trapframe);
    thread_trapframe(t, 0);
    // vmprint(p->mm->pagetable, 1, 0, 0, 0);
    release(&t->lock);

    return p;
}

// free a existed proc
void free_proc(struct proc *p) {
    free_mm(p->mm, p->tg->thread_idx);
    if (p->tg)
        kfree((void *)p->tg);
    p->tg = 0;
    p->pid = 0;
    p->parent = 0;
    p->name[0] = 0;
    p->killed = 0;
    p->exit_state = 0;

    PCB_Q_changeState(p, PCB_UNUSED);
}

struct file console;
void _user_init(void) {
    struct proc *p = NULL;

    p = create_proc();
    initproc = p;

    safestrcpy(p->name, "/init", 10);
    p->mm->brk = 0;

    console.f_type = T_DEVICE;
    console.f_mode = O_RDWR;
    console.f_major = CONSOLE;

    TCB_Q_changeState(p->tg->group_leader, TCB_RUNNABLE);
    release(&p->lock);
    return;
}

// init proc based on FAT32 file system
void init_ret(void) {
    extern struct _superblock fat32_sb;
    fat32_fs_mount(ROOTDEV, &fat32_sb); // initialize fat32 superblock obj and root inode obj.
    proc_current()->_cwd = fat32_inode_dup(fat32_sb.root);
    proc_current()->tg->group_leader->trapframe->a0 = do_execve("/boot/init", NULL, NULL);
}

// initialize the proc table.
void proc_init(void) {
    struct proc *p;
    atomic_set(&next_pid, 1);
    atomic_set(&count_pid, 0);

    PCB_Q_ALL_INIT();

    for (int i = 0; i < NPROC; i++) {
        p = proc + i;
        sprintf(proc_lock_name[i], "proc_%d", i);
        initlock(&p->lock, proc_lock_name[i]);

        p->state = PCB_UNUSED;
        PCB_Q_push_back(&unused_p_q, p);
    }
    return;
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

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void thread_forkret(void) {
    // Still holding p->lock from scheduler.
    release(&thread_current()->lock);
    if (thread_current() == initproc->tg->group_leader) {
        init_ret();
    }
    thread_usertrapret();
}

int do_clone(int flags, uint64 stack, pid_t ptid, uint64 tls, pid_t *ctid) {
    int pid;
    struct proc *p = proc_current();
    struct proc *np = NULL;
    struct tcb *t = NULL;

    if (flags & CLONE_THREAD) {
        if ((t == alloc_thread()) == 0) {
            return -1;
        }
        proc_join_thread(p, t);
        // wait semaphore
        sema_init(&t->sem_wait_chan_parent, 0, "thread_sema_chan");

        // uvm_thread_trapframe(t->p->mm->pagetable, 0, (paddr_t)t->trapframe);
        thread_trapframe(t, 0);
        release(&t->lock);
        t->trapframe->a0 = 0;

        // TODO : append a thread to a proc
        return p->pid;
    } else {
        // Allocate process.
        if ((np = create_proc()) == 0) {
            return -1;
        }
    }

    // copy saved user registers.
    *(np->tg->group_leader->trapframe) = *(p->tg->group_leader->trapframe);

    // Cause fork to return 0 in the child.
    np->tg->group_leader->trapframe->a0 = 0;

    /* Copy vma */
    if (vmacopy(p->mm, np->mm) < 0) {
        free_proc(np);
        release(&np->lock);
        return -1;
    }

    // Copy user memory from parent to child.
    if (flags & CLONE_VM) {
        np->mm->pagetable = p->mm->pagetable;
    } else {
        if (uvmcopy(p->mm, np->mm) < 0) {
            free_proc(np);
            release(&np->lock);
            return -1;
        }
    }

    np->mm->start_brk = p->mm->start_brk;
    np->mm->brk = p->mm->brk;
    // increment reference counts on open file descriptors.
    if (flags & CLONE_FILES) {
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
        np->tg->group_leader->pending = p->tg->group_leader->pending;
    } else {
        // TODO : create a new signal
    }

    // store the parent pid
    if (flags & CLONE_PARENT_SETTID) {
        if (either_copyin(&ptid, 1, p->pid, sizeof(pid_t)) == -1)
            return -1;
    }
    // set the tls (Thread-local Storage，TLS)
    // RISC-V使用TP寄存器
    if (flags & CLONE_SETTLS) {
        np->tg->group_leader->trapframe->tp = tls;
    }

    // 子线程中存储子线程 ID 的变量指针
    if (flags & CLONE_CHILD_SETTID) {
        // np->ctid = *ctid;
        // np->set_child_tid = ctid;
        np->tg->group_leader->set_child_tid = ctid;
    }

    if (flags & CLONE_CHILD_CLEARTID) {
        np->ctid = 0;
        // np->clear_child_tid = ctid;
        np->tg->group_leader->clear_child_tid = ctid;
    }

    if (stack) {
        np->tg->group_leader->trapframe->sp = stack;
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

    acquire(&np->tg->group_leader->lock);
    TCB_Q_changeState(np->tg->group_leader, TCB_RUNNABLE);
    release(&np->tg->group_leader->lock);

    release(&np->lock);
    return pid;
}

// Exit the current process.
void do_exit(int status) {
    struct proc *p = proc_current();
    struct tcb *t = thread_current();

    if (p == initproc)
        panic("init exiting");

    // Close all open files.
    for (int fd = 0; fd < NOFILE; fd++) {
        if (p->_ofile[fd]) {
            struct file *f = p->_ofile[fd];
            generic_fileclose(f);
            p->_ofile[fd] = 0;
        }
    }
    fat32_inode_put(p->_cwd);
    p->_cwd = 0;

    // release all threads
    proc_release_all_thread(p);

    // Give any children to init.
    reparent(p);

    acquire(&p->lock);

    p->exit_state = status << 8;

    PCB_Q_changeState(p, PCB_ZOMBIE);

    //  only valid for the group leader of proc
    sema_signal(&p->parent->tg->group_leader->sem_wait_chan_parent);
    sema_signal(&p->tg->group_leader->sem_wait_chan_self);

#ifdef __DEBUG_PROC__
    printfGreen("exit : %d has exited\n", p->pid);                // debug
    printfGreen("exit : %d wakeup %d\n", p->pid, p->parent->pid); // debug
#endif
    release(&p->lock);

    acquire(&t->lock);
    thread_sched();
    panic("existed thread is scheduled");
    return;
}

int waitpid(pid_t pid, uint64 status, int options) {
    struct proc *p = proc_current();
    if (pid < -1)
        pid = -pid;

    ASSERT(pid != 0);

    if (nochildren(p)) {
#ifdef __DEBUG_PROC__
        printf("wait : %d hasn't children\n", p->pid); // debug
#endif
        return -1;
    }

    if (proc_killed(p)) {
#ifdef __DEBUG_PROC__
        printf("wait : %d has been killed\n", p->pid); // debug
#endif
        return -1;
    }
    while (1) {
        sema_wait(&p->tg->group_leader->sem_wait_chan_parent);
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
                sema_wait(&p_child->tg->group_leader->sem_wait_chan_self);
#ifdef __DEBUG_PROC__
                printfBlue("wait : %d wakeup self\n", p->pid); // debug
#endif
            }
            acquire(&p_child->lock);
            if (p_child->state == PCB_ZOMBIE) {
                // if(p==initproc)
                //     printfRed("唤醒,pid : %d, %d\n",p_child->pid, ++cnt_wakeup); // debug

                // ASSERT(p_child->pid!=SHELL_PID);
                pid = p_child->pid;
                if (status != 0 && copyout(p->mm->pagetable, status, (char *)&(p_child->exit_state), sizeof(p_child->exit_state)) < 0) {
                    release(&p_child->lock);
                    return -1;
                }
                free_proc(p_child);

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
        // printf("%d\n", p->sem_wait_chan_parent.value);
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
            if (p_child->state == PCB_ZOMBIE) {
                sema_signal(&initproc->tg->group_leader->sem_wait_chan_parent); // !!!!!
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

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int proc_kill(int pid) {
    struct proc *p;
    if ((p = find_get_pid(pid)) == NULL)
        return -1;
    p->killed = 1;
    release(&p->lock);

#ifdef __DEBUG_PROC__
    printfCYAN("kill : kill %d\n", p->pid); // debug
#endif

    //  wakeup all sleeping thread
    proc_wakeup_all_thread(p);
    // if (p->state == PCB_SLEEPING) {
    //     // Wake process from sleep().
    //     PCB_Q_changeState(p, PCB_RUNNABLE);
    // }
    return 0;
}

void proc_setkilled(struct proc *p) {
    acquire(&p->lock);
    p->killed = 1;
    release(&p->lock);
}

int proc_killed(struct proc *p) {
    int k;

    acquire(&p->lock);
    k = p->killed;
    release(&p->lock);
    return k;
}

uint8 get_current_procs() {
    // TODO : add lock to proc table??
    uint8 procs = 0;
    struct proc *p;
    for (p = proc; p < &proc[NPROC]; p++) {
        acquire(&p->lock);
        if (p->state != PCB_UNUSED) {
            procs++;
        }
        release(&p->lock);
    }
    return procs;
}

void proc_thread_print(void) {
    struct proc *p;
    // char *state;
    for (p = proc; p < &proc[NPROC]; p++) {
        // pass
    }
}
