#include "common.h"
#include "param.h"
#include "memory/memlayout.h"
#include "riscv.h"
#include "proc/pcb_life.h"
#include "kernel/cpu.h"
#include "test.h"
#include "memory/vm.h"
#include "proc/sched.h"

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
void iinit();
void fileinit(void);
void vmas_init();
#ifdef KCSAN
void kcsaninit();
#endif
void mm_init();
__attribute__((aligned(16))) char stack0[4096 * NCPU];


int debug_lock = 0;
// start() jumps here in supervisor mode on all CPUs.
void main() {
    if (cpuid() == 0) {
        consoleinit();
        printfinit();
        debug_lock = 1;
        printf("\n");
        printf("xv6 kernel is booting\n");
        printf("\n");

        mm_init();
        vmas_init();
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

#ifdef KCSAN
        kcsaninit();
#endif
        __sync_synchronize();
        started = 1;
    } else {
        while (atomic_read4((int *)&started) == 0)
            ;
        __sync_synchronize();
        printf("hart %d starting\n", cpuid());
        kvminithart();  // turn on paging
        trapinithart(); // install kernel trap vector
        plicinithart(); // ask PLIC for device interrupts
    }

    scheduler();
}
