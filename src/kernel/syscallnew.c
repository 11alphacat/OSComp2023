#include "debug.h"
#include "common.h"
#include "proc/pcb_life.h"

// oscomp syscalls that haven't been implemented

// proc
uint64 sys_exit_group(void) {
    return 0;
}

// uid_t geteuid(void);
uint64 sys_geteuid(void) {
    return 0;
}

uint64 sys_gettid(void) {
    return 0;
}

uint64 sys_prlimit64(void) {
    return 0;
}

uint64 sys_rt_sigtimedwait(void) {
    return 0;
}