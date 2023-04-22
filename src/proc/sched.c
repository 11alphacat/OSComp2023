#include "proc/pcb_queue.h"
#include "proc/sched.h"

// UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE
PCB_Q_t unused_q, used_q, running_q, runnable_q, sleeping_q, zombie_q;

void PCB_Q_ALL_INIT(){
    PCB_Q_init(&unused_q, "UNUSED");
    PCB_Q_init(&used_q, "USED");
    PCB_Q_init(&running_q, "RUNNING");
    PCB_Q_init(&runnable_q, "RUNNABLE");
    PCB_Q_init(&sleeping_q, "SLEEPING");
    PCB_Q_init(&zombie_q, "ZOMBIE");
}
