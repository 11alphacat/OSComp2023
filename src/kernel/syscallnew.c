#include "debug.h"
#include "common.h"
#include "proc/pcb_life.h"

// oscomp syscalls that haven't been implemented

// other

// memory
uint64 sys_mprotect(void) {
    ASSERT(0);
    return 0;
}

// proc

// filesystem
uint64 sys_utimensat(void) {
    ASSERT(0);
    return 0;
}

uint64 sys_ioctl(void) {
    ASSERT(0);
    return 0;
}

uint64 sys_renameat2(void) {
    ASSERT(0);
    return 0;
}

uint64 sys_statfs(void) {
    ASSERT(0);
    return 0;
}
