#ifndef __SIGNAL_H__
#define __SIGNAL_H__
#include "proc/pcb_life.h"

int kill(int);
void setkilled(struct proc *);
int killed(struct proc *);


#endif