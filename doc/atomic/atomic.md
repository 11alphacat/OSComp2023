# 原子指令

##### 为什么需要一个原子操作的数据结构？

多线程程序中，当两个或多个线程试图同时修改同一个数据时，就有可能导致数据不一致或其他问题。原子操作可以保证临界资源的读和写操作要么发生，要么不发生，不会出现数据竞争的情况。

原子操作主要是用在一些简单的计数器上，实现对每个计数器原子地进行读-修改-写，主要是硬件层面的实现，我们只需要用RISCV指令集提供的原子指令即可。





##### 实现一个原子操作需要定义什么数据结构和接口？

```c
// 原子操作数据结构
typedef struct {
    volatile int counter;
} atomic_t;
// 初始化
#define ATOMIC_INITIALIZER \
    { 0 }
#define ATOMIC_INIT(i) \
    { (i) }
// 原子读取和写入
#define WRITE_ONCE(var, val) \
    (*((volatile typeof(val) *)(&(var))) = (val))
#define READ_ONCE(var) (*((volatile typeof(var) *)(&(var))))
#define atomic_read(v) READ_ONCE((v)->counter)
#define atomic_set(v, i) WRITE_ONCE(((v)->counter), (i))
// 自增 带返回加法前的原始值
static inline int atomic_inc_return(atomic_t *v) {
    return __sync_fetch_and_add(&v->counter, 1);
}
// 自减 带返回减法前的原始值
static inline int atomic_dec_return(atomic_t *v) {
    return __sync_fetch_and_sub(&v->counter, 1);
}
```

这里我们直接使用GCC提供的内置函数，在RISC-V下会被翻译为对应的原子指令。下面是其内联汇编的实现。

```c
// atomic_inc_return
static inline unsigned int __sync_fetch_and_add(volatile unsigned int *ptr, unsigned int val) {
    unsigned int temp;
    asm volatile(
        "amoswap.w.aq	%0, %2, (%1)"
        : "=r"(temp)
        : "r"(ptr), "r"(val)
        : "memory"
    );
    return temp;
}
// __sync_fetch_and_sub
static inline unsigned int __sync_fetch_and_sub(volatile unsigned int *ptr, unsigned int val) {
    val = -val;
    unsigned int temp;
    asm volatile(
        "amoswap.w.aq	%0, %2, (%1)"
        : "=r"(temp)
        : "r"(ptr), "r"(val)
        : "memory"
    );
    return temp;
}
```



##### 哪些场景下需要使用这些原子指令？

1. 自旋锁

```c
while (__sync_lock_test_and_set(&lk->locked, 1) != 0)
    ;
```

2. 进程next_pid的增加和减少

```c
#define alloc_pid (atomic_inc_return(&next_pid))
#define cnt_pid_inc (atomic_inc_return(&count_pid))
#define cnt_pid_dec (atomic_dec_return(&count_pid))
```

3. 线程next_tid的增加和减少

```c
#define alloc_tid (atomic_inc_return(&next_tid))
#define cnt_tid_inc (atomic_inc_return(&count_tid))
#define cnt_tid_dec (atomic_dec_return(&count_tid))
```







