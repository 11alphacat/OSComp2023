#ifndef __SCHED_H__
#define __SCHED_H__
#include "proc/pcb_queue.h"

void PCB_Q_ALL_INIT();

void yield(void);
void sched(void);
void scheduler(void) __attribute__((noreturn));

#endif