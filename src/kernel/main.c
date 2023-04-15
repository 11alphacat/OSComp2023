#include "common.h"
#include "param.h"
#include "memory/memlayout.h"
#include "riscv.h"
#include "kernel/proc.h"
#include "kernel/cpu.h"
#include "fs/fat/fat32_mem.h"

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
void inode_table_init(void);

__attribute__((aligned(16))) char stack0[4096 * NCPU];

// start() jumps here in supervisor mode on all CPUs.
void main() {
    if (cpuid() == 0) {
        consoleinit();
        printfinit();

        printf("\nxv6 kernel is booting\n\n");

        // // buddy and slab
        // phy_mm_init();

        // KVM
        kinit();       // physical page allocator
        kvminit();     // create kernel page table
        kvminithart(); // turn on paging

        // Proc management
        procinit(); // process table

        // Trap
        trapinit();     // trap vectors
        trapinithart(); // install kernel trap vector

        // PLIC
        plicinit();     // set up interrupt controller
        plicinithart(); // ask PLIC for device interrupts

        // File System
        binit(); // buffer cache

        // origin
        iinit();    // inode table
        fileinit(); // file table
        inode_table_init();

        // fat32
        // fat32_fat_entry_init();

        // virtual disk
        virtio_disk_init(); // emulated hard disk

        // First user process
        userinit(); // first user process
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
