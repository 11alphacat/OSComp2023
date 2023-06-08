#include "debug.h"
#include "common.h"
#include "proc/pcb_life.h"

// oscomp syscalls that haven't been implemented

// proc
uint64 sys_exit_group(void) {
    return 0;
}

// filesystem
