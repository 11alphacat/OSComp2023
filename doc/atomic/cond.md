# 等待队列重写条件变量

##### 为什么需要写一个cond条件变量的实现？

条件变量(Condition Variable)提供了一系列原语，使得当前线程在等待的某一条件不能满足时停止使用CPU并将自己挂起。在挂起状态，操作系统调度器不再选择当前线程执行。直到等待的条件满足，其他线程才会唤醒该挂起的线程继续执行。使用条件变量能够有效避免无谓的循环等待。

Xv6的sleep和wakeup就是条件变量的思想，但不支持等待队列，且实现不规范，所以我们重写了一个条件变量的实现。





##### 实现条件变量需要什么数据结构和接口？

```c
struct cond {
    struct Queue waiting_queue;
};

void cond_init(struct cond *cond, char *name);
void cond_wait(struct cond *cond, 
               struct spinlock *mutex);
void cond_signal(struct cond *cond);
void cond_broadcast(struct cond *cond);
```

条件变量提供了三个接口：

- cond_wait用于挂起当前线程以等待在对应的条件变量上。
- cond_signal用于唤醒一个等待在该条件变量上的线程。
- cond_broadcast则用于唤醒所有等待在该条件变量上的线程。

条件变量必须搭配一个互斥锁一起使用，该互斥锁用于保护对条件的判断与修改。

cond_wait 接收两个参数，分别为条件变量与搭配使用的互斥锁。在调用cond_wait时，需要保证当前线程已经获取搭配的互斥锁（在临界区中）。cond_wait在挂起当前线程的同时，该互斥锁同时被释放。其他线程则有机会进入临界区，满足该线程等待的条件，然后利用cond_signal唤醒该线程。挂起线程被唤醒之后，会在cond_wait返回之前重新获取互斥锁。

cond_signal只接收一个参数：当前使用的条件变量。虽然cond_signal没有明确要求必须持有搭配的互斥锁（在临界区中），但在临界区外使用cond_signal需要非常小心才能保证不丢失唤醒。

cond_broadcast与cond signal类似，唯一的区别是cond_broadcast会唤醒所有等待在该条件变量上的线程，而cond_signal只会唤醒其中的一个。 

实现思路和xv6一样，但通过等待队列实现，舍弃了原始的通过chan字段进行暴力查找的方法。即我们舍弃了chan字段，全部使用等待队列。

 

##### 为什么一定要先挂起，然后再放锁？

如果先放锁，然后再挂起，那么其他线程可能再放锁和挂起的间隙调用cond_siganl操作，然后导致线程丢失唤醒，例如下面的情况：

| 时刻  | 线程0         | 线程1       |
| ----- | ------------- | ----------- |
| $T_0$ | unlock(mutex) |             |
| $T_1$ |               | lock(mutex) |
| $T_2$ |               | cond_signal |
| $T_3$ | 阻塞这个线程  |             |

由于线程0还没有挂起休眠，那么cond_siganl就会无法对其起作用，即sleep无法被signal唤醒，所以必须保证先阻塞这个线程，然后释放锁，如下图。

| 时刻  | 线程0         | 线程1       |
| ----- | ------------- | ----------- |
| $T_0$ | 阻塞这个线程  |             |
| $T_1$ | unlock(mutex) |             |
| $T_2$ |               | lock(mutex) |
| $T_3$ |               | cond_signal |





##### 内核代码中哪些地方使用了cond条件变量？

1. 各种信号量，内部都是通过条件变量的队列实现的。

```c
struct semaphore {
    volatile int value;
    volatile int wakeup;
    spinlock_t sem_lock;
    struct cond sem_cond;
};
```

2. 保护ticks的锁

```c
extern struct cond cond_ticks;
```

如果线程已经被杀死了，还处于SLEEPING状态，那么除了改变线程的状态外，还需要将其从等待队列中删除，否则这个线程就留在等待队列中了，这显然是不对的。

```c
if (info->si_signo == SIGKILL || info->si_signo == SIGSTOP) {
    if (t_cur->state == TCB_SLEEPING) {
        Queue_remove_atomic(&cond_ticks.waiting_queue, (void *)t_cur); // bug
        TCB_Q_changeState(t_cur, TCB_RUNNABLE);
    }
}
```

决赛中代码中修改如下：

```c
if (t_cur->state == TCB_SLEEPING) {
    thread_wakeup(t_cur);
}
// holding lock
void thread_wakeup(struct tcb *t) {
    ASSERT(t->wait_chan_entry != NULL);
    Queue_remove_atomic(t->wait_chan_entry, (void *)t);
    ASSERT(t->state == TCB_SLEEPING);
    t->wait_chan_entry = NULL;
    TCB_Q_changeState(t, TCB_RUNNABLE);
}
```

接口更加通用！

在调用线程的休眠的时候，会间这个线程插入等待队列。

```c
cond_wait(&cond_ticks, &tickslock);
```

每次发生时钟中断的时候，需要将所有在这个等待队列下的线程全部唤醒：

```c
cond_broadcast(&cond_ticks);
```

ppoll 中需要使用这个条件变量实现定时休眠

```c
if (timeout) {
    extern struct cond cond_ticks;
    acquire(&cond_ticks.waiting_queue.lock);
    cond_wait(&cond_ticks, &cond_ticks.waiting_queue.lock);
    release(&cond_ticks.waiting_queue.lock);
    timeout--;
} else {
    break;
}
```

clock_nanosleep中也需要使用到cond_wait实现定时休眠

```c
if (flags == 0) {
    do_sleep_ns(t, request);
} else if (flags == TIMER_ABSTIME) {
    uint64 request_ns = TIMESEPC2NS(request);
    uint64 now_ns = TIME2NS(rdtime());
    if (now_ns >= request_ns) {
        return 0;
    } else {
        acquire(&cond_ticks.waiting_queue.lock);
        t->time_out = request_ns - now_ns;
        cond_wait(&cond_ticks, &cond_ticks.waiting_queue.lock);
        release(&cond_ticks.waiting_queue.lock);
    }
}
```

pdflush 需要使用这个条件变量实现pdflush内核线程的休眠

```c
int wait_ret = cond_wait(&pdflush_control.pdflush_cond, &pdflush_control.lock);
```





##### 需要注意的点：

1. 在进行sched前一定要持有锁。
2. 进行队列的压入操作和弹出操作时最好获取锁，即采用带atomic后缀的接口，防止并发bug。
3. signal和broadcast一个是唤醒一个，一个是唤醒全部。
4. 在进行状态切换的时候一定要注意获取进程的锁。











