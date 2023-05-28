#include "debug.h"
#include "common.h"

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

uint64 sys_faccessat(void) {
    ASSERT(0);
    return 0;
}

uint64 sys_renameat2(void) {
    ASSERT(0);
    return 0;
}

uint64 sys_sendfile(void) {
    ASSERT(0);
    return 0;
}

uint64 sys_statfs(void) {
    ASSERT(0);
    return 0;
}

uint64 sys_utimensat(void) {
    ASSERT(0);
    return 0;
}

uint64 sys_lseek(void) {
    ASSERT(0);
    return 0;
}

uint64 sys_writev(void) {
    ASSERT(0);
    return 0;
}

uint64 sys_readv(void) {
    ASSERT(0);
    return 0;
}

uint64 sys_fcntl(void) {
    ASSERT(0);
    return 0;
}
