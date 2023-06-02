#ifndef __WAIT_QUEUE_H__
#define __WAIT_QUEUE_H__
#include "atomic/spinlock.h"
#include "lib/list.h"

struct tcb;
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

// is empty (atomic)?
int Waiting_Q_isempty_atomic(Waiting_Q_t *waiting_q);

// push back
void Waiting_Q_push_back(Waiting_Q_t *waiting_q, struct tcb *t);

// move it from its old waiting QUEUE
void Waiting_Q_remove(struct tcb *t);

// move it from its old waiting QUEUE (atomic)
void Waiting_Q_remove_atomic(Waiting_Q_t *waiting_q, struct tcb *t);

// pop the queue
struct tcb *Waiting_Q_pop(Waiting_Q_t *waiting_q);

// provide the first one of the queue
struct tcb *Waiting_Q_provide(Waiting_Q_t *Waiting_Q);

#endif