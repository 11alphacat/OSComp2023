#ifndef __SCHED_H__
#define __SCHED_H__
#include "proc/pcb_life.h"
#include "proc/pcb_thread.h"
#include "lib/queue.h"

struct proc;
struct tcb;

void PCB_Q_ALL_INIT(void);
void PCB_Q_changeState(struct proc *, enum procstate);

void TCB_Q_ALL_INIT(void);
void TCB_Q_changeState(struct tcb *t, enum thread_state state_new);

void thread_yield(void);
void thread_sched(void);
void thread_scheduler(void) __attribute__((noreturn));

#endif