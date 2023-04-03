#include "common.h"
#include "param.h"
#include "memory/memlayout.h"
#include "riscv.h"
#include "kernel/proc.h"
#include "kernel/cpu.h"

volatile static int started = 0;
void printfinit(void);
void consoleinit(void);
void trapinit(void);
void trapinithart(void);
void kvminit(void);
void kvminithart(void);
void plicinit(void);
void plicinithart(void);
void virtio_disk_init(void);
void binit(void);
void userinit(void);
void kinit(void);
void iinit();
void fileinit(void);

__attribute__((aligned(16))) char stack0[4096 * NCPU];

// start() jumps here in supervisor mode on all CPUs.
void main() {
    if (cpuid() == 0) {
        consoleinit();
        printfinit();
        printf("\n");
        printf("xv6 kernel is booting\n");
        printf("\n");
        kinit();            // physical page allocator
        kvminit();          // create kernel page table
        kvminithart();      // turn on paging
        procinit();         // process table
        trapinit();         // trap vectors
        trapinithart();     // install kernel trap vector
        plicinit();         // set up interrupt controller
        plicinithart();     // ask PLIC for device interrupts
        binit();            // buffer cache
        iinit();            // inode table
        fileinit();         // file table
        virtio_disk_init(); // emulated hard disk
        userinit();         // first user process
        __sync_synchronize();
        started = 1;
    } else {
        while (started == 0)
            ;
        __sync_synchronize();
        printf("hart %d starting\n", cpuid());
        kvminithart();  // turn on paging
        trapinithart(); // install kernel trap vector
        plicinithart(); // ask PLIC for device interrupts
    }

    scheduler();
}
