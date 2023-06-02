#include "debug.h"
#include "common.h"
#include "proc/pcb_life.h"

// oscomp syscalls that haven't been implemented

// memory
uint64 sys_mprotect(void) {
    ASSERT(0);
    return 0;
}

// filesystem
uint64 sys_ioctl(void) {
    ASSERT(0);
    return 0;
}
