#include "common.h"
#include "param.h"
#include "memory/memlayout.h"
#include "riscv.h"
#include "atomic/spinlock.h"
#include "proc/pcb_life.h"
#include "proc/pcb_thread.h"
#include "kernel/trap.h"
#include "syscall_gen/syscall_num.h"

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

void syscall(void) {
    int num;
    // struct proc *p = proc_current();
    struct tcb *t = thread_current();

    num = t->trapframe->a7;
    if (num >= 0 && num < NELEM(syscalls) && syscalls[num]) {
        // Use num to lookup the system call function for num, call it,
        // and store its return value in p->trapframe->a0
        t->trapframe->a0 = syscalls[num]();
    } else {
        printf("tid : %d name : %s: unknown sys call %d\n",
               t->tid, t->name, num);
        t->trapframe->a0 = -1;
    }
}