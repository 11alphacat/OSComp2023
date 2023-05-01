#ifndef __WAIT_QUEUE_H__
#define __WAIT_QUEUE_H__
#include "atomic/spinlock.h"
#include "list.h"

struct proc;
struct Waiting_Q {
    struct spinlock lock;
    struct list_head list;
    char name[12]; // the name of queue
};
typedef struct Waiting_Q Waiting_Q_t;

// init
void Waiting_Q_init(Waiting_Q_t *waiting_q, char *name);

// is empty?
int Waiting_Q_isempty(Waiting_Q_t *waiting_q);

// push back
void Waiting_Q_push_back(Waiting_Q_t *waiting_q, struct proc *p);

// move it from its old waiting QUEUE
void Waiting_Q_remove(struct proc *p);

// pop the queue
struct proc *Waiting_Q_pop(Waiting_Q_t *waiting_q);

// provide the first one of the queue
struct proc *Waiting_Q_provide(Waiting_Q_t *Waiting_Q);

#endif