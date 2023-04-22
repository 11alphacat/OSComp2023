#ifndef __PCB_QUEUE_H__
#define __PCB_QUEUE_H__
#include "common.h"
#include "atomic/spinlock.h"
#include "list.h"
#include "kernel/proc.h"

struct proc;

struct PCB_Q{
    struct spinlock lock;
    struct list_head list;
    char name[12];// the name of queue
};
typedef struct PCB_Q PCB_Q_t;

// init
static inline void PCB_Q_init(PCB_Q_t* pcb_q, char* name)
{
    initlock(&pcb_q->lock,name);
    INIT_LIST_HEAD(&pcb_q->list);
    strncpy(pcb_q->name, name, strlen(name));
}

// is empty?
static inline int PCB_Q_isempty(PCB_Q_t* pcb_q)
{
    return list_empty(&(pcb_q->list));
}

// push back
static inline void PCB_Q_push_back(PCB_Q_t* pcb_q, struct proc* p)
{
    list_add_tail(&(p->PCB_list), &(pcb_q->list));
}


// move it from its old PCB QUEUE
static inline void PCB_Q_remove(struct proc* p)
{
    list_del(&p->PCB_list);
    INIT_LIST_HEAD(&(p->PCB_list));
}
// pop the queue
static inline struct proc* PCB_Q_pop(PCB_Q_t* pcb_q)
{
    if(PCB_Q_isempty(pcb_q))
        return NULL;
    struct proc* p = list_first_entry(&pcb_q->list, struct proc, PCB_list);
    PCB_Q_remove(p);
    return p;
}

#endif