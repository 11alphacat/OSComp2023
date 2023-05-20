#include "proc/wait_queue.h"
#include "proc/pcb_life.h"
#include "proc/tcb_queue.h"
#include "list.h"

// init
void Waiting_Q_init(Waiting_Q_t *waiting_q, char *name) {
    initlock(&waiting_q->lock, name);
    INIT_LIST_HEAD(&waiting_q->list);
    strncpy(waiting_q->name, name, strlen(name));
}

// is empty?
int Waiting_Q_isempty(Waiting_Q_t *waiting_q) {
    // acquire(&waiting_q->lock);
    int empty = list_empty(&(waiting_q->list));
    // release(&waiting_q->lock);
    return empty;
}

// is empty (atomic)?
int Waiting_Q_isempty_atomic(Waiting_Q_t *waiting_q) {
    acquire(&waiting_q->lock);
    int empty = list_empty(&(waiting_q->list));
    release(&waiting_q->lock);
    return empty;
}

// push back
void Waiting_Q_push_back(Waiting_Q_t *waiting_q, struct tcb *t) {
    acquire(&waiting_q->lock);
    list_add_tail(&(t->wait_list), &(waiting_q->list));
    release(&waiting_q->lock);
}

// move it from its old waiting QUEUE
void Waiting_Q_remove(struct tcb *t) {
    list_del(&t->wait_list);
    INIT_LIST_HEAD(&(t->wait_list));
}

// pop the queue
struct tcb *Waiting_Q_pop(Waiting_Q_t *waiting_q) {
    if (Waiting_Q_isempty(waiting_q))
        return NULL;
    struct tcb *t = list_first_entry(&(waiting_q->list), struct tcb, wait_list);
    Waiting_Q_remove(t);
    return t;
}

// provide the first one of the queue
struct tcb *Waiting_Q_provide(Waiting_Q_t *Waiting_Q) {
    struct tcb *t;
    acquire(&Waiting_Q->lock);
    t = Waiting_Q_pop(Waiting_Q);
    release(&Waiting_Q->lock);
    // may return NULL
    return t;
}
