#include "debug.h"
#include "common.h"

// oscomp syscalls that haven't been implemented

uint64 sys_umount2(void) {
    // ASSERT(0);
    return 0;
}

uint64 sys_mount(void) {
    // ASSERT(0);
    return 0;
}
