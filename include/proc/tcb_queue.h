#ifndef __TCB_QUEUE_H__
#define __TCB_QUEUE_H__

#include "common.h"
#include "lib/list.h"
#include "atomic/spinlock.h"
#include "proc/pcb_thread.h"

struct tcb;

struct TCB_Q {
    struct spinlock lock;
    struct list_head list;
    char name[12]; // the name of queue
};
typedef struct TCB_Q TCB_Q_t;

// init
static inline void TCB_Q_init(TCB_Q_t *tcb_q, char *name) {
    initlock(&tcb_q->lock, name);
    INIT_LIST_HEAD(&tcb_q->list);
    strncpy(tcb_q->name, name, strlen(name));
}

// is empty?
static inline int TCB_Q_isempty(TCB_Q_t *tcb_q) {
    return list_empty(&(tcb_q->list));
}

// push back
static inline void TCB_Q_push_back(TCB_Q_t *tcb_q, struct tcb *t) {
    list_add_tail(&(t->state_list), &(tcb_q->list));
}

// move it from its old TCB QUEUE
static inline void TCB_Q_remove(struct tcb *t) {
    list_del(&t->state_list);
    INIT_LIST_HEAD(&(t->state_list));
}

// pop the queue
static inline struct tcb *TCB_Q_pop(TCB_Q_t *tcb_q, int remove) {
    if (TCB_Q_isempty(tcb_q))
        return NULL;
    struct tcb *t = list_first_entry(&(tcb_q->list), struct tcb, state_list);
    if (remove)
        TCB_Q_remove(t);
    return t;
}

// provide the first one of the queue
static inline struct tcb *TCB_Q_provide(TCB_Q_t *tcb_q, int remove) {
    struct tcb *t;
    acquire(&tcb_q->lock);
    t = TCB_Q_pop(tcb_q, remove);
    release(&tcb_q->lock);
    return t;
}

#endif