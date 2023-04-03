#ifndef __CPU_H__
#define __CPU_H__

#include "common.h"
#include "kernel/kthread.h"
#include "param.h"

struct proc;
// Per-CPU state.
struct cpu {
    struct proc *proc;      // The process running on this cpu, or null.
    struct context context; // swtch() here to enter scheduler().
    int noff;               // Depth of push_off() nesting.
    int intena;             // Were interrupts enabled before push_off()?
};

extern struct cpu cpus[NCPU];

int cpuid(void);
struct cpu *mycpu(void);
struct cpu *getmycpu(void);

#endif // __CPU_H__