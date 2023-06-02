#include "kernel/plic.h"
#include "kernel/trap.h"
#include "kernel/cpu.h"
#include "memory/vm.h"
#include "memory/memlayout.h"
#include "proc/pcb_life.h"
#include "proc/signal.h"
#include "proc/sched.h"
#include "lib/riscv.h"
#include "lib/sbi.h"
#include "driver/uart.h"
#include "driver/virtio.h"
#include "atomic/spinlock.h"
#include "atomic/cond.h"
#include "atomic/semaphore.h"
#include "lib/queue.h"
#include "common.h"
#include "param.h"
#include "debug.h"


#define SET_TIMER() sbi_legacy_set_timer(*(uint64 *)CLINT_MTIME + CLINT_INTERVAL)

struct spinlock tickslock;
uint ticks;
struct cond cond_ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void trapinit(void) {
    initlock(&tickslock, "time");
    cond_init(&cond_ticks, "cond_ticks");
}

// set up to take exceptions and traps while in the kernel.
void trapinithart(void) {
    w_stvec((uint64)kernelvec);
    // start timer
    w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);
    SET_TIMER();
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void thread_usertrap(void) {
    int which_dev = 0;

    if ((r_sstatus() & SSTATUS_SPP) != 0)
        panic("usertrap: not from user mode");

    // send interrupts and exceptions to kerneltrap(),
    // since we're now in the kernel.
    w_stvec((uint64)kernelvec);

    struct proc *p = proc_current();
    struct tcb *t = thread_current();

    // save user program counter.
    t->trapframe->epc = r_sepc();

    if (r_scause() == SYSCALL) {
        // system call

        if (thread_killed(t))
            do_exit(-1);

        // sepc points to the ecall instruction,
        // but we want to return to the next instruction.
        t->trapframe->epc += 4;

        // an interrupt will change sepc, scause, and sstatus,
        // so enable only now that we're done with those registers.
        intr_on();

        syscall();
    } else if ((which_dev = devintr()) != 0) {
        // ok
    } else {
        // vmprint(p->pagetable, 0, 0, 0, 0);
        // pte_t *pte;
        // walk(p->pagetable, r_stval(), 0, 0, &pte);
        // PTE("RSW %d%d U %d X %d W %d R %d\n",
        //     (*pte & PTE_READONLY) > 0, (*pte & PTE_SHARE) > 0,
        //     (*pte & PTE_U) > 0, (*pte & PTE_X) > 0,
        //     (*pte & PTE_W) > 0, (*pte & PTE_R) > 0);
        uint64 cause = r_scause();
        if (cause == INSTUCTION_PAGEFAULT
            || cause == LOAD_PAGEFAULT
            || cause == STORE_PAGEFAULT) {
            if (pagefault(cause, p->pagetable, r_stval()) < 0) {
                printf("process %s\n", p->name);
                printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
                printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
                proc_sendsignal_all_thread(p, SIGKILL, 1);
            }
        } else {
            printf("process %s\n", p->name);
            printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
            printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
            proc_sendsignal_all_thread(p, SIGKILL, 1);
        }
    }

    if (thread_killed(t))
        do_exit(-1);

    // give up the CPU if this is a timer interrupt.
    if (which_dev == 2)
        thread_yield();

    // handle the signal
    signal_handle(t);

    thread_usertrapret();
}

//
// return to user space
//
void thread_usertrapret() {
    struct proc *p = proc_current();
    struct tcb *t = thread_current();
    // we're about to switch the destination of traps from
    // kerneltrap() to usertrap(), so turn off interrupts until
    // we're back in user space, where usertrap() is correct.
    intr_off();

    // send syscalls, interrupts, and exceptions to uservec in trampoline.S
    uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
    w_stvec(trampoline_uservec);

    // set up trapframe values that uservec will need when
    // the process next traps into the kernel.

    t->trapframe->kernel_satp = r_satp();         // kernel page table
    t->trapframe->kernel_sp = t->kstack + PGSIZE; // process's kernel stack
    t->trapframe->kernel_trap = (uint64)thread_usertrap;
    t->trapframe->kernel_hartid = r_tp(); // hartid for cpuid()

    // set up the registers that trampoline.S's sret will use
    // to get to user space.

    // set S Previous Privilege mode to User.
    unsigned long x = r_sstatus();
    x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
    x |= SSTATUS_SPIE; // enable interrupts in user mode
    w_sstatus(x);

    // set S Exception Program Counter to the saved user pc.
    w_sepc(t->trapframe->epc);

    // tell trampoline.S the user page table to switch to.
    uint64 satp = MAKE_SATP(p->pagetable);

    // jump to userret in trampoline.S at the top of memory, which
    // switches to the user page table, restores user registers,
    // and switches to user mode with sret.
    uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
    ((void (*)(uint64))trampoline_userret)(satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void kerneltrap() {
    int which_dev = 0;
    uint64 sepc = r_sepc();
    uint64 sstatus = r_sstatus();
    uint64 scause = r_scause();

    if ((sstatus & SSTATUS_SPP) == 0)
        panic("kerneltrap: not from supervisor mode");
    if (intr_get() != 0)
        panic("kerneltrap: interrupts enabled");

    if ((which_dev = devintr()) == 0) {
        backtrace();
        printf("scause %p\n", scause);
        printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
        panic("kerneltrap");
    }

    // give up the CPU if this is a timer interrupt.
    if (which_dev == 2 && thread_current() != 0 && thread_current()->state == TCB_RUNNING)
        thread_yield();

    // the yield() may have caused some traps to occur,
    // so restore trap registers for use by kernelvec.S's sepc instruction.
    w_sepc(sepc);
    w_sstatus(sstatus);
}

void clockintr() {
    acquire(&tickslock);
    ticks++;
    cond_broadcast(&cond_ticks);
    // wakeup(&tickslock);
    release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int devintr() {
    uint64 scause = r_scause();

    if ((scause & 0x8000000000000000L) && (scause & 0xff) == 9) {
        // this is a supervisor external interrupt, via PLIC.

        // irq indicates which device interrupted.
        int irq = plic_claim();

        if (irq == UART0_IRQ) {
            uartintr();
        } else if (irq == VIRTIO0_IRQ) {
            virtio_disk_intr();
        } else if (irq) {
            printf("unexpected interrupt irq=%d\n", irq);
        }

        // the PLIC allows each device to raise at most one
        // interrupt at a time; tell the PLIC the device is
        // now allowed to interrupt again.
        if (irq)
            plic_complete(irq);

        return 1;

    } else if (scause == 0x8000000000000005L) {
        if (cpuid() == 0) {
            clockintr();
        }
        SET_TIMER();

        return 2;
    } else {
        return 0;
    }
}
