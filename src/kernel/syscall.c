#include "common.h"
#include "param.h"
#include "memory/memlayout.h"
#include "riscv.h"
#include "atomic/spinlock.h"
#include "proc/pcb_life.h"
#include "proc/pcb_thread.h"
#include "kernel/trap.h"
#include "syscall_gen/syscall_num.h"
#include "debug.h"

// Fetch the uint64 at addr from the current process.
int fetchaddr(vaddr_t addr, uint64 *ip) {
    struct proc *p = proc_current();
    if (addr >= p->sz || addr + sizeof(uint64) > p->sz) // both tests needed, in case of overflow
        return -1;
    if (copyin(p->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
        return -1;
    return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Returns length of string, not including nul, or -1 for error.
int fetchstr(uint64 addr, char *buf, int max) {
    struct proc *p = proc_current();
    if (copyinstr(p->pagetable, buf, addr, max) < 0)
        return -1;
    return strlen(buf);
}

static uint64
argraw(int n) {
    // struct proc *p = proc_current();
    struct tcb *t = thread_current();
    switch (n) {
    case 0:
        return t->trapframe->a0;
    case 1:
        return t->trapframe->a1;
    case 2:
        return t->trapframe->a2;
    case 3:
        return t->trapframe->a3;
    case 4:
        return t->trapframe->a4;
    case 5:
        return t->trapframe->a5;
    }
    panic("argraw");
    return -1;
}

// Fetch the nth 32-bit system call argument.
int argint(int n, int *ip) {
    *ip = argraw(n);
    if (*ip < 0)
        return -1;
    else
        return 0;
}

// int arglong(int n, uint64 *ip) {
//     *ip = argraw(n);
//     if (*ip < 0)
//         return -1;
//     else
//         return 0;
// }

void argulong(int n, unsigned long *ulip) {
    *ulip = argraw(n);
}

void arglong(int n, long *lip) {
    *lip = argraw(n);
}

// Retrieve an argument as a pointer.
// Doesn't check for legality, since
// copyin/copyout will do that.
int argaddr(int n, uint64 *ip) {
    *ip = argraw(n);
    if (*ip < 0)
        return -1;
    else
        return 0;
}

// Fetch the nth word-sized system call argument as a null-terminated string.
// Copies into buf, at most max.
// Returns string length if OK (including nul), -1 if error.
int argstr(int n, char *buf, int max) {
    uint64 addr;
    argaddr(n, &addr);
    return fetchstr(addr, buf, max);
}

// Prototypes for the functions that handle system calls.
#include "syscall_gen/syscall_def.h"

// An array mapping syscall numbers from syscall.h
// to the function that handles the system call.
static uint64 (*syscalls[])(void) = {
#include "syscall_gen/syscall_func.h"
};

#ifdef __STRACE__
struct syscall_info {
    const char *name;
    int num;
    // s/p/d/...
    char type[5]; /* reserve a space for \0 */
};

static struct syscall_info info[] = {
    /* pid_t getpid(void); */
    [SYS_getpid] { "getpid", 0, },
    // int exit(int) __attribute__((noreturn));
    [SYS_exit] { "exit", 1, "d" },
    /* int execve(const char *pathname, char *const argv[], char *const envp[]); */
    [SYS_execve] { "execve", 3, "spp" },
    // char* sbrk(int);
    [SYS_sbrk] { "sbrk", 1, "d" },
    /* int close(int fd); */
    [SYS_close] { "close", 1, "d" },
    /* ssize_t read(int fd, void *buf, size_t count); */
    [SYS_read] { "read", 3, "dpu" },
    // int open(const char*, int);
    [SYS_openat] { "openat", 4, "dsxx" },
    /* int getdents64(unsigned int fd, struct linux_dirent64 *dirp,
                    unsigned int count); */
    [SYS_getdents64] { "getdents64", 3, "upu" },
    // int write(int, const void*, int);
    [SYS_write] { "write", 3, "dpd" },

    // // int fork(void);
    // [SYS_fork] { "fork", 0, },
    // // int wait(int*);
    // [SYS_wait] { "wait", 1, "p" },
    // // int pipe(int*);
    // [SYS_pipe] { "pipe", 1, "p" },
    // // int kill(int);
    // [SYS_kill] { "kill", 1, "d" },
    // // int fstat(int fd, struct stat*);
    // [SYS_fstat] { "fstat", 2, "dp" },
    // // int chdir(const char*);
    // [SYS_chdir] { "chdir", 1, "s" },
    // // int dup(int);
    // [SYS_dup] { "dup", 1, "d" },
    // // int sleep(int);
    // [SYS_sleep] { "sleep", 1, "d" },
    // // int uptime(void);
    // [SYS_uptime] { "uptime", 0 },
    // // int mknod(const char*, short, short);
    // [SYS_mknod] { "mknod", 3, "sdd" },
    // // int unlink(const char*);
    // [SYS_unlink] { "unlink", 1, "s" },
    // // int link(const char*, const char*);
    // [SYS_link] { "link", 2, "ss" },
    // // int mkdir(const char*);
    // [SYS_mkdir] { "mkdir", 1, "s" },
};

#define STRACE_TARGET_NUM 1
// cannot use to debug pr(printf's lock)!!!
char *strace_proc_name[STRACE_TARGET_NUM] = {
    "ls",
};

int is_strace_target() {
    /* trace all proc except sh and init */
    if (proc_current()->pid > 2) {
        return 1;
    } else {
        return 0;
    }
    // for (int i = 0; i < STRACE_TARGET_NUM; i++) {
    //     if (strncmp(proc_current()->name, strace_proc_name[i], sizeof(strace_proc_name[i])) == 0) {
    //         return 1;
    //     }
    // }
    // return 0;
}

#endif

void syscall(void) {
    int num;
#ifdef __STRACE__
    /* a0 use both in argument and return value, so need to preserve it when open STRACE */
    int a0;
    struct proc *p = proc_current();
#endif

    struct tcb *t = thread_current();

    num = t->trapframe->a7;
    if (num >= 0 && num < NELEM(syscalls) && syscalls[num]) {
        // Use num to lookup the system call function for num, call it,
        // and store its return value in p->trapframe->a0
#ifdef __STRACE__
        a0 = t->trapframe->a0;
        if (is_strace_target()) {
            STRACE("%d: syscall %s(", p->pid, info[num].name);
            for (int i = 0; i < info[num].num; i++) {
                uint64 argument;
                switch (i) {
                case 0: argument = a0; break;
                case 1: argument = t->trapframe->a1; break;
                case 2: argument = t->trapframe->a2; break;
                case 3: argument = t->trapframe->a3; break;
                case 4: argument = t->trapframe->a4; break;
                case 5: argument = t->trapframe->a5; break;
                default: panic("could not reach here"); break;
                }
                switch (info[num].type[i]) {
                case 's': {
                    char buf[100];
                    copyinstr(p->pagetable, buf, argument, 100);
                    STRACE("%s, ", buf);
                    break;
                }
                case 'd': STRACE("%d, ", argument); break;
                case 'p': STRACE("%p, ", argument); break;
                case 'u': STRACE("%u, ", argument); break;
                case 'x': STRACE("%#x, ", argument); break;
                default: STRACE("\\, "); break;
                }
            }
        }
#endif
        t->trapframe->a0 = syscalls[num]();
#ifdef __STRACE__
        if (is_strace_target()) {
            STRACE(") -> %d\n", t->trapframe->a0);
        }
#endif
    } else {
        printf("tid : %d name : %s: unknown sys call %d\n",
               t->tid, t->name, num);
        t->trapframe->a0 = -1;
    }
}