#include "proc/wait_queue.h"
#include "list.h"

// init
void Waiting_Q_init(Waiting_Q_t *waiting_q, char *name) {
    initlock(&waiting_q->lock, name);
    INIT_LIST_HEAD(&waiting_q->list);
    strncpy(waiting_q->name, name, strlen(name));
}

// is empty?
int Waiting_Q_isempty(Waiting_Q_t *waiting_q) {
    return list_empty(&(waiting_q->list));
}

// push back
void Waiting_Q_push_back(Waiting_Q_t *waiting_q, struct proc *p) {
    list_add_tail(&(p->wait_list), &(waiting_q->list));
}

// move it from its old waiting QUEUE
void Waiting_Q_remove(struct proc *p) {
    list_del(&p->wait_list);
    INIT_LIST_HEAD(&(p->wait_list));
}

// pop the queue
struct proc *Waiting_Q_pop(Waiting_Q_t *waiting_q) {
    if (Waiting_Q_isempty(waiting_q))
        return NULL;
    struct proc *p = list_first_entry(&(waiting_q->list), struct proc, wait_list);
    Waiting_Q_remove(p);
    return p;
}

// provide the first one of the queue
struct proc *Waiting_Q_provide(Waiting_Q_t *Waiting_Q) {
    struct proc *p;
    acquire(&Waiting_Q->lock);
    p = Waiting_Q_pop(Waiting_Q);
    release(&Waiting_Q->lock);
    // may return NULL
    return p;
}
