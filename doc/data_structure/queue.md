# 内核通用数据结构——queue

##### 为什么需要一个通用的队列数据结构？

原始xv6的代码进行进程调度的时候，每次都需要遍历整个全局的进程数组，然后判断是否这个进程的状态为某个特定的值，然后进行一系列的处理。这样的处理十分低效，会带来多次获取锁和无效的遍历。

如下：

1. allocproc

```c
if(p->state == UNUSED) {

} else {

}
```

2. waitpid

```c
if(pp->state == ZOMBIE){
    ...
}
```

3. scheduler

```c
if(p->state == RUNNABLE) {
...
}
```

4. wakup 和 kill

```c
if(p->state == SLEEPING && p->chan == chan) {
    p->state = RUNNABLE;
}

if(p->state == SLEEPING){
    // Wake process from sleep().
    p->state = RUNNABLE;
}
```

为了解决这个问题，我们需要加入一个通用的队列数据结构，减少不必要的查表开销，需要什么状态的进程就去某个特定的队列查找。同时也方便cond、semaphore和futex的等待队列的实现。

即内核中需要给进程的每个状态设置一个状态队列，给线程的每个状态设置一个状态队列（除了RUNNING状态），然后留出一个通用的接口实现等待队列。



##### 实现一个通用的队列queue需要定义什么数据结构和接口？

```c
enum queue_type { TCB_STATE_QUEUE,
                  PCB_STATE_QUEUE,
                  TCB_WAIT_QUEUE };
struct Queue {
    struct spinlock lock;
    struct list_head list;
    char name[12]; // the name of queue
    enum queue_type type;
};
typedef struct Queue Queue_t;
// init
void Queue_init(Queue_t *q, char *name, enum queue_type type);
// is empty?
int Queue_isempty(Queue_t *q);
// is empty (atomic)?
int Queue_isempty_atomic(Queue_t *q);
// acquire the list entry of node
struct list_head *queue_entry(void *node, enum queue_type type);
// acquire the first node of queue given type of queue
void *queue_first_node(Queue_t *q);
// push back
void Queue_push_back(Queue_t *q, void *node);
// push back (atomic)
void Queue_push_back_atomic(Queue_t *q, void *node);
// move it from its old Queue
void Queue_remove(void *node, enum queue_type type);
// move it from its old Queue (atomic)
void Queue_remove_atomic(Queue_t *q, void *node);
// pop the queue
void *Queue_pop(Queue_t *q, int remove);
// provide the first one of the queue (atomic)
void *Queue_provide_atomic(Queue_t *q, int remove);
```

我们还是利用list.h来实现这个数据结构，需要注意几点：

1. queue_type用来表示有多少种不同的队列。分别是线程的状态队列、进程的状态队列和线程的等待队列。
2. 接口的后缀为atomic表示这个是一个自带锁的操作，是原子的。一定要想清楚是否需要是原子的操作，不然会触发各种并发bug。
3. 主要就是push_back和pop两个接口实现了队列的先进先出的特性。



##### 内核中哪些具体场景需要用到队列？

1. 条件变量cond的实现

```c
struct cond {
    struct Queue waiting_queue;
};
```

2. futex的实现

```c
struct futex {
    struct spinlock lock;
    struct Queue waiting_queue;
};
```

3. 信号量semaphore中使用了cond，所以间接使用了队列

```c
struct semaphore {
    volatile int value;
    volatile int wakeup;
    spinlock_t sem_lock;
    struct cond sem_cond;
};
```

4. 进程状态切换和线程调度 sched

```c
void PCB_Q_changeState(struct proc *, enum procstate);
void TCB_Q_changeState(struct tcb *t, enum thread_state state_new);
```



##### 补充：

使用一个全局的队列指针数组就可以方便的进行状态切换。

```c
Queue_t *P_STATES[PCB_STATEMAX] = {
    [PCB_UNUSED] & unused_p_q,
    [PCB_USED] & used_p_q,
    [PCB_ZOMBIE] & zombie_p_q};
Queue_t *T_STATES[TCB_STATEMAX] = {
    [TCB_UNUSED] & unused_t_q,
    [TCB_USED] & used_t_q,
    [TCB_RUNNABLE] & runnable_t_q,
    [TCB_SLEEPING] & sleeping_t_q};
```

