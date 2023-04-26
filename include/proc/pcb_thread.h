#ifndef __PCB_THREAD_H__
#define __PCB_THREAD_H__

#include "kernel/kthread.h"
#include "list.h"
enum thread_status { UNUSED,
                     READY,
                     RUNNING,
                     ZOMBIE };
typedef enum thread_status thread_status_t;

struct tcb {
    struct context context;
    struct proc *p;
    uint64 kstack;
    int exit_status;
    thread_status_t t_status;

    struct list_head threads; // thread list
};

// // 1. create
// int pthread_create();

// // 2. exit
// void pthread_exit();

// // 3. join
// int pthread_join();

// // 4. detach
// int pthread_detach();

#endif