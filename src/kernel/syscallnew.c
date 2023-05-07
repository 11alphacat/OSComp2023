#include "debug.h"
#include "common.h"

// oscomp syscalls that haven't been implemented

// uint64 sys_getdents64(void) {
// 	ASSERT(0);
// 	return 0;
// }

// uint64 sys_linkat(void) {
// 	ASSERT(0);
// 	return 0;
// }

uint64 sys_umount2(void) {
    ASSERT(0);
    return 0;
}

uint64 sys_mount(void) {
    ASSERT(0);
    return 0;
}

// uint64 sys_execve(void) {
// 	ASSERT(0);
// 	return 0;
// }

// uint64 sys_wait4(void) {
// ASSERT(0);
// return 0;
// }

// uint64 sys_exit(void) {
// 	int n;
//     argint(0, &n);
//     exit(n);

//     return 0; // not reached
// }

uint64 sys_brk(void) {
    ASSERT(0);
    return 0;
}
uint64 sys_times(void) {
    ASSERT(0);
    return 0;
}

uint64 sys_uname(void) {
    ASSERT(0);
    return 0;
}

uint64 sys_sched_yield(void) {
    ASSERT(0);
    return 0;
}

uint64 sys_gettimeofday(void) {
    ASSERT(0);
    return 0;
}

uint64 sys_nanosleep(void) {
    ASSERT(0);
    return 0;
}
