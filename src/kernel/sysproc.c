#include "common.h"
#include "riscv.h"
#include "param.h"
#include "memory/memlayout.h"
#include "atomic/spinlock.h"
#include "proc/pcb_life.h"
#include "kernel/trap.h"
#include "memory/vm.h"
#include "memory/allocator.h"
#include "debug.h"
#include "proc/pcb_mm.h"
#include "proc/cond.h"
#include "proc/signal.h"
#include "proc/exec.h"
#include "proc/pcb_thread.h"

#define ROOT_UID 0
/*
 * 功能：获取进程ID；
 * 输入：系统调用ID；
 * 返回值：成功返回进程ID；
 */
uint64
sys_getpid(void) {
    return proc_current()->pid;
}

uint64 sys_exit(void) {
    int n;
    argint(0, &n);
    do_exit(n);
    return 0; // not reached
}

/*
 * 功能：获取父进程ID；
 * 输入：系统调用ID；
 * 返回值：成功返回父进程ID；
 */
uint64
sys_getppid(void) {
    acquire(&proc_current()->lock);
    uint64 ppid = proc_current()->parent->pid;
    release(&proc_current()->lock);
    return ppid;
}

/*
* 功能：创建一个子进程；
* 输入：
  - flags: 创建的标志，如SIGCHLD；
  - stack: 指定新进程的栈，可为0；
  - ptid: 父线程ID；
  - tls: TLS线程本地存储描述符；
  - ctid: 子线程ID；
* 返回值：成功则返回子进程的线程ID，失败返回-1；
*/
// int flags, void* stack , pid_t ptid, void*tls, pid_t* ctid
uint64
sys_clone(void) {
    int flags;
    uint64 stack;
    pid_t ptid;
    uint64 tls;
    uint64 ctid;
    argint(0, &flags);
    argaddr(1, &stack);
    argint(2, &ptid);
    argaddr(3, &tls);
    argaddr(4, &ctid);
    return do_clone(flags, stack, ptid, tls, (pid_t *)ctid);
}

/*
* 功能：等待进程改变状态;
* 输入：
  - pid: 指定进程ID，可为-1等待任何子进程；
  - status: 接收状态的指针；
  - options: 选项：WNOHANG，WUNTRACED，WCONTINUED；
* 返回值：成功则返回进程ID；如果指定了WNOHANG，且进程还未改变状态，直接返回0；失败则返回-1；
*/
// pid_t pid, int *status, int options;
uint64
sys_wait4(void) {
    pid_t p;
    uint64 status;
    int options;
    argint(0, &p);
    argaddr(1, &status);
    argint(2, &options);

    return waitpid(p, status, options);
}

/*
* 功能：执行一个指定的程序；
* 输入：
  - path: 待执行程序路径名称，
  - argv: 程序的参数，
  - envp: 环境变量的数组指针
* 返回值：成功不返回，失败返回-1；
*/
uint64 sys_execve(void) {
    char path[MAXPATH];
    vaddr_t uargv, uenvp;
    paddr_t argv, envp;
    vaddr_t temp;

    /* fetch the path str */
    if (argstr(0, path, MAXPATH) < 0) {
        return -1;
    }

    /* fetch the paddr of char **argv and char **envp */
    argaddr(1, &uargv);
    argaddr(2, &uenvp);
    if (uargv == 0) {
        argv = 0;
    } else {
        /* check if the argv parameters is legal */
        for (int i = 0;; i++) {
            if (i >= MAXARG) {
                return -1;
            }
            if (fetchaddr(uargv + sizeof(vaddr_t) * i, (vaddr_t *)&temp) < 0) {
                return -1;
            }
            if (temp == 0) {
                break;
            }
            paddr_t cp;
            if ((cp = walkaddr(proc_current()->pagetable, temp)) == 0 || strlen((const char *)cp) > PGSIZE) {
                return -1;
            }
        }

        argv = getphyaddr(proc_current()->pagetable, uargv);
    }

    if (uenvp == 0) {
        envp = 0;
    } else {
        /* check if the envp parameters is legal */
        for (int i = 0;; i++) {
            if (i >= MAXENV) {
                return -1;
            }
            if (fetchaddr(uenvp + sizeof(vaddr_t) * i, (vaddr_t *)&temp) < 0) {
                return -1;
            }
            if (temp == 0) {
                break;
            }
            vaddr_t cp;
            if ((cp = walkaddr(proc_current()->pagetable, temp)) == 0 || strlen((const char *)cp) > PGSIZE) {
                return -1;
            }
        }

        envp = getphyaddr(proc_current()->pagetable, uenvp);
    }

    return do_execve(path, (char *const *)argv, (char *const *)envp);
}

uint64 sys_sbrk(void) {
    uint64 addr;
    int n;

    argint(0, &n);
    addr = proc_current()->sz;
    if (growproc(n) < 0)
        return -1;
    return addr;
}

uint64 sys_brk(void) {
    uintptr_t oldaddr;
    uintptr_t newaddr;
    intptr_t increment;

    oldaddr = proc_current()->sz;
    argaddr(0, &newaddr);
    /*  contest requirement: brk(0) return the proc_current location of the program break
        This is different from the behavior of the brk interface in Linux
    */
    if (newaddr == 0) {
        return oldaddr;
    }
    increment = (intptr_t)newaddr - (intptr_t)oldaddr;

    if (growproc(increment) < 0)
        return -1;
    return oldaddr;
}

uint64 sys_print_pgtable(void) {
    struct proc *p = proc_current();
    vmprint(p->pagetable, 1, 0, 0, 0);
    uint64 memsize = get_free_mem();
    Log("%dM", memsize / 1024 / 1024);
    return 0;
}

uint64
sys_kill(void) {
    int pid;

    argint(0, &pid);
    return proc_kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void) {
    uint xticks;

    acquire(&tickslock);
    xticks = ticks;
    release(&tickslock);
    return xticks;
}

// getuid() returns the real user ID of the calling process.
// uid_t getuid(void);
uint64 sys_getuid(void) {
    return ROOT_UID;
}

// pid_t set_tid_address(int *tidptr);
uint64 sys_set_tid_address(void) {
    uint64 tidptr;
    argaddr(0, &tidptr);
    struct proc *p = proc_current();
    struct tcb *t = thread_current();

    t->clear_child_tid = (int *)getphyaddr(p->pagetable, tidptr);

    return t->tid;
}

// int rt_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact, size_t sigsetsize);
// examine and change a signal action
uint64 sys_rt_sigaction(void) {
    int signum;
    size_t sigsetsize;

    uint64 act_addr;
    uint64 oldact_addr;
    struct sigaction act;
    struct sigaction oldact;
    argint(0, &signum);
    argaddr(1, &act_addr);
    argaddr(2, &oldact_addr);
    argulong(3, &sigsetsize);

    int ret;

    // struct sigaction act_real;

    if (sigsetsize != sizeof(sigset_t))
        return -1;

    struct proc *p = proc_current();
    // If act is non-NULL, the new action for signal signum is installed from act
    if (act_addr) {
        if (copyin(p->pagetable, (char *)&act, act_addr, sizeof(act)) < 0) {
            return -1;
        }
        // if (copyin(p->pagetable, (char *)&act_real.sa_handler, (uint64)act.sa_handler, sizeof(act.sa_handler)) < 0) {
        //     return -1;
        // }
    }

    ret = do_sigaction(signum, act_addr ? &act : NULL, oldact_addr ? &oldact : NULL);

    // If oldact is non-NULL, the previous action is saved in oldact
    if (!ret && oldact_addr) {
        if (copyout(p->pagetable, oldact_addr, (char *)&oldact, sizeof(oldact)) < 0) {
            return -1;
        }
    }

    return ret;
}

// int rt_sigprocmask(int how, const kernel_sigset_t *set, kernel_sigset_t *oldset, size_t sigsetsize);
uint64 sys_rt_sigprocmask(void) {
    int how;
    uint64 set_addr;
    uint64 oldset_addr;
    size_t sigsetsize;
    argint(0, &how);
    argaddr(1, &set_addr);
    argaddr(2, &oldset_addr);
    argulong(3, &sigsetsize);

    sigset_t set;
    sigset_t old_set;

    int ret = 0;

    if (sigsetsize != sizeof(sigset_t))
        return -1;

    // If set is NULL, then the signal mask is unchanged (i.e., how is ignored),
    // but the current value of the signal mask is nevertheless returned in oldset
    if (set_addr) {
        if (copyin(proc_current()->pagetable, (char *)&set, set_addr, sizeof(set)) < 0) {
            return -1;
        }
    }
    sig_del_set_mask(set, sig_gen_mask(SIGKILL) | sig_gen_mask(SIGSTOP));
    ret = do_sigprocmask(how, &set, &old_set);

    // If oldset is non-NULL, the previous value of the signal mask is stored in oldset
    if (!ret && oldset_addr) {
        if (copyin(proc_current()->pagetable, (char *)&old_set, oldset_addr, sizeof(old_set)) < 0) {
            return -1;
        }
    }
    return ret;
}

// return from signal handler and cleanup stack frame
uint64 sys_rt_sigreturn(void) {
    struct tcb *t = thread_current();

    signal_trapframe_restore(t);
    return 0;
}