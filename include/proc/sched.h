#ifndef __SCHED_H__
#define __SCHED_H__
#include "proc/pcb_queue.h"
#include "proc/pcb_life.h"

struct proc;

void PCB_Q_ALL_INIT();
void PCB_Q_changeState(struct proc*, enum procstate);
void yield(void);
void sched(void);
void scheduler(void) __attribute__((noreturn));


#endif