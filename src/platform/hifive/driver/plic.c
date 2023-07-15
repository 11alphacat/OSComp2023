#include "common.h"
#include "kernel/plic.h"
#include "param.h"
// #include "memory/memlayout.h"
#include "platform/hifive/pml_hifive.h"
#include "lib/riscv.h"
#include "kernel/cpu.h"
#include "proc/pcb_life.h"
//
// the riscv Platform Level Interrupt Controller (PLIC).
//

void plicinit(void) {
    // set desired IRQ priorities non-zero (otherwise disabled).
    // *(uint32 *)(PLIC + UART0_IRQ * 4) = 1;
    *(uint32 *)(PLIC_PRIORITY(UART0_IRQ)) = 1;

    // TODO

}

void plicinithart(void) {
    int hart = cpuid();

    // set enable bits for this hart's S-mode
    // for the uart and PDMA(TODO)
    ((uint32 *)PLIC_SENABLE(hart))[ UART0_IRQ / 32 ] |= UART0_IRQ % 32;    

    // set this hart's S-mode priority threshold to 0.
    *(uint32 *)PLIC_SPRIORITY(hart) = 0;
}

// ask the PLIC what interrupt we should serve.
int plic_claim(void) {
    int hart = cpuid();
    int irq = *(uint32 *)PLIC_SCLAIM(hart);
    return irq;

}

// tell the PLIC we've served this IRQ.
void plic_complete(int irq) {
    int hart = cpuid();
    *(uint32 *)PLIC_SCLAIM(hart) = irq;
}
