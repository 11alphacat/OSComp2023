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

extern struct spinlock wait_lock;

/*
 * 功能：获取进程ID；
 * 输入：系统调用ID；
 * 返回值：成功返回进程ID；
 */
uint64
sys_getpid(void) {
    return current()->pid;
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
    acquire(&current()->lock);
    uint64 ppid = current()->parent->pid;
    release(&current()->lock);
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
// const char *path, char *const argv[], char *const envp[];
// uint64 sys_execve(void) {
//     char path[MAXPATH];
//     char *argv[MAXARG];
//     char *envp[MAXENV];

//     if (argstr(0, path, MAXPATH) < 0) {
//         return -1;
//     }
//     // TODO : 获取一个字符串数组 （argv 和 envp）
//     return do_execve(path, argv, envp);
// }

uint64 sys_sbrk(void) {
    uint64 addr;
    int n;

    argint(0, &n);
    addr = current()->sz;
    if (growproc(n) < 0)
        return -1;
    return addr;
}
uint64 sys_brk(void) {
    uintptr_t oldaddr;
    uintptr_t newaddr;
    intptr_t increment;

    oldaddr = current()->sz;
    argaddr(0, &newaddr);
    /*  contest requirement: brk(0) return the current location of the program break
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
    struct proc *p = current();
    vmprint(p->pagetable, 0, 0, 0, 0);
    uint64 memsize = get_free_mem();
    Log("%dM", memsize / 1024 / 1024);
    return 0;
}

uint64
sys_sleep(void) {
    int n;
    uint ticks0;

    argint(0, &n);
    acquire(&tickslock);
    ticks0 = ticks;
    while (ticks - ticks0 < n) {
        if (killed(current())) {
            release(&tickslock);
            return -1;
        }
        sleep(&ticks, &tickslock);
    }
    release(&tickslock);
    return 0;
}

uint64
sys_kill(void) {
    int pid;

    argint(0, &pid);
    return kill(pid);
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
