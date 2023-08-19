#### fuex 是什么？

futex - fast user-space locking，即快速用户空间的锁。它是一种用于用户空间应用程序的通用同步工具（基于futex可以在userspace实现互斥锁、读写锁、condition variable等同步机制）。在无竞争的情况下操作完全在user space进行，不需要系统调用，仅在发生竞争的时候进入内核去完成相应的处理(wait 或者 wake up)。futex 通过用户态和内核配合，可以减小开销，并且线程灵活可控是否要进入睡眠等待还是spin等待。



#### futex的主要接口和其行为？

```c
struct futex *get_futex(uint64 uaddr, int assert);
void futex_init(struct futex *fp, char *name);
int futex_wait(uint64 uaddr, uint val, struct timespec *ts);
int futex_wakeup(uint64 uaddr, int nr_wake);
int futex_requeue(uint64 uaddr1, int nr_wake, uint64 uaddr2, int nr_requeue);
int do_futex(uint64 uaddr, int op, uint32 val, struct timespec *ts, 
             uint64 uaddr2, uint32 val2, uint32 val3);
```

do_futex主要是sys_futex 的主要调用接口，根据传入的op进行不同的操作：

```c
switch (cmd) {
    case FUTEX_WAIT:
        ret = futex_wait(uaddr, val, ts);
        break;

    case FUTEX_WAKE:
        ret = futex_wakeup(uaddr, val);
        break;

    case FUTEX_REQUEUE:
        ret = futex_requeue(uaddr, val, uaddr2, val2); 
        // must use val2 as a limit of requeue
        break;
    default:
        panic("do_futex : error\n");
        break;
}
```

本次比赛主要需要实现就是三个功能：

futex_wait、futex_wake和futex_requeue。

和内核信号量的接口十分相似（典型的PV操作）：

```c
void sema_wait(sem *S);
void sema_signal(sem *S);
```

futex_init用于初始化一个futex等待队列：

```c
Queue_init(&fp->waiting_queue, name, TCB_WAIT_QUEUE);
```

get_futex主要用户通过全局的futex_map实现从uint64的用户态地址uaddr到接口体指针的映射。

1. 通过hash_lookup 找到futex队列。
2. 通过hash_insert 将uaddr和futex构建<key，value>插入hash表。

futex_free千万别忘记，主要是调用hash_delete来防止内存泄漏。

```c
hash_delete(&futex_map, (void *)uaddr, 0, 1); 
```

futex_wait主要的代码就是：

```c
if (u_val == val) {
	// 和cond_wait十分类似，只是多了一个定时唤醒的功能
} else {
    // 释放锁，返回即可
}
```

futex_wakeup 的实现思路和cond_signal十分类似，不过只能唤醒最多nr_wake个线程，还有就是如果发现队列空了，要调用futex_free！

futex_requeue的思想如下：

1. 先通过futex_wakeup 唤醒uaddr1对应队列上的nr_wakeup 个线程。
2. 将uaddr1 队列上的最多nr_requeue个线程转移到uaddr2对应的队列上。





#### futex需要注意的点？

1. get_futex 一定要注意保证原子性，可以使用线程组的spinlock，也可以使用进程的spinlock。
2. 需要先实现一个带队列的定时器，futex_wait就好实现了。
3. futex_wakeup 需要注意nr_wake这个变量对于唤醒线程数量的限制！
4. futex_requeue 需要注意 nr_requeue这个变量对于转移线程数量的限制！
5. 如果实现了队列的基本接口，基于内核级信号量 semaphore 就十分容易实现futex的wait、wakeup和requeue。





#### 关于futex最大的问题？

那就是

```c
/* CLONE_CHILD_SETTID: */
uint64 set_child_tid;
/* CLONE_CHILD_CLEARTID: */
uint64 clear_child_tid;
```

这两个字段需要认真实现！

1. alloc_thread中完成这两个字段的初始化。

```c
// for clone
t->set_child_tid = 0;
t->clear_child_tid = 0;
```

2. do_clone 中完成clear_child_tid的设置。

```c
if (flags & CLONE_CHILD_CLEARTID) {
    // when the child exits,  Clear (zero)
    t->clear_child_tid = ctid;
}
```

3. set_tid_address 需要保存clear_child_tid字段。

```c
uint64 sys_set_tid_address(void) {
    struct tcb *t = thread_current();
    t->clear_child_tid = t->trapframe->a0;
    return t->tid;
}
```

在线程exit的时候一定要检查一下这个字段，用于线程唤醒：

```c
void exit_wakeup(struct proc *p, struct tcb *t) {
    // bug !!!
    if (t->clear_child_tid) {
        int val = 0;
        if (copyout(p->mm->pagetable, t->clear_child_tid, (char *)&val, sizeof(val)))
            panic("exit error\n");
        futex_wakeup(t->clear_child_tid, 1);
    }
}
```















