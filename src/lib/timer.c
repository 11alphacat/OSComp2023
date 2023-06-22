#include "lib/timer.h"
#include "lib/riscv.h"
#include "lib/list.h"
#include "memory/memlayout.h"
#include "atomic/cond.h"
#include "atomic/ops.h"

struct timer_entry timer_head;
struct spinlock tickslock;
struct cond cond_ticks;
atomic_t ticks;

void timer_init() {
    atomic_set(&ticks, 0);
    cond_init(&cond_ticks, "cond_ticks");
    timer_entry_init(&timer_head, "timer_entry");
}

void timer_entry_init(struct timer_entry *t_entry, char *name) {
    initlock(&t_entry->lock, name);
    INIT_LIST_HEAD(&t_entry->entry);
}

// expires : ns!!!
void add_timer_atomic(struct timer_list *timer, uint64 expires, timer_expire function, void *data) {
    timer->data = data;
    timer->expires_end = expires + TIME2NS(rdtime());
    timer->expires = expires;
    timer->function = function;
    INIT_LIST_HEAD(&timer->list);

    acquire(&timer_head.lock);
    list_add_tail(&timer->list, &timer_head.entry);
    release(&timer_head.lock);
}

void delete_timer_atomic(struct timer_list *timer) {
    acquire(&timer_head.lock);
    list_del_reinit(&timer->list);
    release(&timer_head.lock);
}

void timer_list_decrease_atomic(struct timer_entry *head) {
    struct timer_list *timer_cur;
    struct timer_list *timer_tmp;

    acquire(&head->lock);
    list_for_each_entry_safe(timer_cur, timer_tmp, &head->entry, list) {
        uint64 time_now_ns = TIME2NS(rdtime());
        if (time_now_ns > timer_cur->expires_end) {
            timer_cur->function(timer_cur->data);
            if (timer_cur->count != -1) {
                list_del_reinit(&timer_cur->list);
                timer_cur->expires_end = 0;
                timer_cur->expires = 0;
            } else {
                timer_cur->expires_end = timer_cur->expires + TIME2NS(rdtime());
            }
        }
    }
    release(&head->lock);
}
void clockintr() {
    atomic_inc_return(&ticks);
    timer_list_decrease_atomic(&timer_head);
}