#include "common.h"
#include "param.h"
#include "memory/memlayout.h"
#include "lib/riscv.h"
#include "proc/pcb_life.h"
#include "kernel/cpu.h"
#include "fs/fat/fat32_mem.h"
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
void fileinit(void);
// void userinit(void);
// void fileinit(void);
void vmas_init();
#ifdef KCSAN
void kcsaninit();
#endif
void mm_init();
void _user_init(void);
void proc_init();
void inode_table_init(void);
void futex_table_init(void);
void hash_tables_init(void);

__attribute__((aligned(16))) char stack0[4096 * NCPU];

int debug_lock = 0;
// start() jumps here in supervisor mode on all CPUs.
void main() {
    if (cpuid() == 0) {
        consoleinit();
        printfinit();
        debug_lock = 1;

        printf("\nOur LostWakeup OS kernel is booting\n\n");

        mm_init();
        vmas_init();
        // KVM
        kvminit();     // create kernel page table
        kvminithart(); // turn on paging

        // Proc management
        proc_init(); // process table
        tcb_init();
        futex_table_init(); // futex table

        // map init
        hash_tables_init();

        // Trap
        trapinit();     // trap vectors
        trapinithart(); // install kernel trap vector

        // PLIC
        plicinit();     // set up interrupt controller
        plicinithart(); // ask PLIC for device interrupts

        // File System
        binit(); // buffer cache

        // origin
        // fileinit(); // file table
        fileinit();
        inode_table_init();

        // fat32
        // fat32_fat_entry_init();

        // virtual disk
        virtio_disk_init(); // emulated hard disk

        // First user process
        _user_init(); // first user process
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

    thread_scheduler();
}
