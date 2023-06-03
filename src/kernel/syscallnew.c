#include "debug.h"
#include "common.h"
#include "proc/pcb_life.h"

// oscomp syscalls that haven't been implemented

// other

// proc
uint64 sys_exit_group(void) {
    return 0;
}

// filesystem
uint64 sys_utimensat(void) {
    ASSERT(0);
    return 0;
}

uint64 sys_ioctl(void) {
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
