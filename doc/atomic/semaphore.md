# 内核信号量

##### 为什么需要实现信号量？

我们在用户态可以使用futex系统调用实现同步，在内核态可以使用内核信号量实现同步来代替所有的sleeplock（信号量初始值为1，实现互斥），代替部分xv6的条件变量实现（信号量初始值为0，实现同步）。这样就可以用信号量实现一个统一的同步接口（虽然有些复杂的同步无法通过信号量实现）。所以，我们利用重写的cond条件变量写了一个较为规范的内核信号量，并在我们的内核中广泛应用。当内核控制路径试图获取内核信号量所保护的忙资源时，相应的进程被挂起。只有在资源被释放时，才再次变为可运行的。





##### 实现信号量需要什么数据结构和通用的接口？

```c
struct semaphore {
    volatile int value;
    volatile int wakeup;
    spinlock_t sem_lock;
    struct cond sem_cond;
};
typedef struct semaphore sem;
void sema_init(sem *S, int value, char *name);
void sema_wait(sem *S);
void sema_signal(sem *S);
```

sema_wait操作用于等待。当信号量的值（代表资源数量）小于等于0时进入循环等待。只有在信号量的值大于0时，才停止等待并消耗该资源（减少信号量的值）。

sema_signal操作用于通知，其会增加信号量的值供 sema_wait 的线程使用。



##### 在内核中哪些地方使用了信号量semaphore？

使用我们实现的内核信号量可以将内核中的sleeplock和条件变量给统一，使用起来十分方便。

1. buffer_head中一个互斥信号量，一个同步信号量。

```c
// buffer_head的互斥信号量
struct semaphore sem_lock;
// buffer virtio_disk_intr是否完成读取
struct semaphore sem_disk_done;
```

2. superblock中的互斥信号量

```c
// 实现对superblock这个全局临界资源的互斥访问
struct semaphore sem
```

3. inode中的互斥信号量

```c
// 对全局inode table中的inode进行互斥访问
struct semaphore i_sem;
```

4. tcb中的两个同步信号量

```c
// 用于没有指定pid的waitpid
struct semaphore sem_wait_chan_parent;
// 用于指定了pid的waitpid
struct semaphore sem_wait_chan_self;
```

5. pipe配合自旋锁lock实现缓冲区的读与写的同步

```c
// 可读同步信号量
struct semaphore read_sem;
// 可写同步信号量
struct semaphore write_sem;
```

6. console配合自旋锁lock实现console的buf的读

```c
// 实现console的buf读的同步(从内核到console的数据流)
struct semaphore sem_r;
```

7. uart配合自旋锁lock实现uart的buf的读取（等价于console的写）

```c
// 实现uart的读的操作
struct semaphore uart_tx_r_sem;
```

8. virtio_disk结合disk_vidsk_lock实现disk的写和读的同步

```c
// 实现disk的度和写的同步
struct semaphore sem_disk;
```



##### 需要注意的点：

1. 如果是同步关系，就初始化为0。
2. 如果是互斥关系，就初始化为1。
3. 内核信号量只能实现每次唤醒一个等待队列中的线程，如果需要唤醒所有，最好使用条件变量cond。



##### TODO：

将pipe用传统生产者消费者模型，信号量才起到了不错的效果，且较为规范。

