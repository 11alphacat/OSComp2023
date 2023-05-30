.section .text
.global _entry
# sbi load the martid in a0
_entry:
        # set up a stack for C.
        # stack0 is declared in start.c,
        # with a 4096-byte stack per CPU.
        # sp = stack0 + (hartid * 4096)

        # keep each CPU's hartid in its tp register, for cpuid().
        mv tp, a0
        la sp, stack0
        li t0, 1024*4
        # load hartid
        mv t1, a0
        addi t1, t1, 1
        mul t0, t0, t1
        add sp, sp, t0
        # jump to start() in start.c
        call main
spin:
        j spin
