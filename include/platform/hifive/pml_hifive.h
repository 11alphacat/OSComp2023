#ifndef __PML_HIFIVE_H__
#define __PML_HIFIVE_H__

// hifive u740 puts UART registers here in physical memory.
// #define UART0_BASE 0x10010000L // UART0 base address
// #define UART1_BASE 0x10011000L // UART1 base address
#define UART0_IRQ 39
#define UART1_IRQ 40

// hifive u740 puts SPI registers here in physical memory.
// #define QSPI_2_BASE ((unsigned int)0x10050000)
#define SPI0_IRQ 41
#define SPI1_IRQ 42
#define SPI2_IRQ 43

// core local interruptor (CLINT), which contains the timer.
#define CLINT 0x2000000L
// #define CLINT_MTIMECMP(hartid) (CLINT + 0x4000 + 8 * (hartid))
#define CLINT_MTIME (CLINT + 0xBFF8) // cycles since boot.
#define CLINT_INTERVAL 1000000       // cycles; about 1/10th second in qemu.

// hifive u740 puts platform-level interrupt controller (PLIC) here. (width=4B)
#define PLIC 0x0c000000L
#define PLIC_PRIORITY(intID) (PLIC + 0x4 * (intID)) // intID starts from 1, ends to 69
#define PLIC_PENDING (PLIC + 0x1000)
#define PLIC_MENABLE(hart) ((hart) > 0 ? PLIC + 0x2080 + (hart)*0x100 : PLIC + 0x2000) // Start Hart 0 M-Mode interrupt enables
#define PLIC_SENABLE(hart) (PLIC + 0x2100 + ((hart)-1) * 0x100)                        //  Start Hart 1 S-Mode interrupt enables

#define PLIC_MPRIORITY(hart) ((hart) > 0 ? PLIC + 0x201000 + (hart)*0x2000 : PLIC + 0x200000)
#define PLIC_MCLAIM(hart) ((hart) > 0 ? PLIC + 0x201004 + (hart)*0x2000 : PLC + 0X200004) // M-Mode claim/complete
#define PLIC_SPRIORITY(hart) (PLIC + 0x202000 + ((hart)-1) * 0x2000)                      // hart starts from 1, ends to 4
#define PLIC_SCLAIM(hart) (PLIC + 0x202004 + ((hart)-1) * 0x2000)                         // S-Mode claim/complete

#endif // __PML_HIFIVE_H__