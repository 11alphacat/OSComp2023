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

4. bcache 的 refcnt 的原子性递增

```c
atomic_inc_return(&b->refcnt);
atomic_dec_return(&b->refcnt);
```

5. socket 端口号递增分配

```c
#define ASSIGN_PORT atomic_inc_return(&PORT)
```

6. 原子性增加和减少当前空闲页数量

```c
atomic_add_return(&pages_cnt, 1 << page->order);
atomic_sub_return(&pages_cnt, 1 << page->order);
```

7. ipc的id分配

```c
#define alloc_ipc_id(ipc_ids) (atomic_inc_return(&((ipc_ids)->next_ipc_id)))
```

8. page ref 的原子性递增和减少

```c
atomic_inc_return(&page->refcnt);
atomic_dec_return(&page->refcnt);
```

9. 线程组的线程数量的原子性递增和减少

```c
atomic_inc_return(&tg->thread_cnt);
if (!(atomic_dec_return(&tg->thread_cnt) - 1)) {
    exit_process(status);
    last_thread = 1;
}
atomic_dec_return(&tg->thread_cnt);
```

10. pdflush 的线程数量增加和递增

```c
atomic_inc_return(&pdflush_control.nr_pdflush_threads);
atomic_dec_return(&pdflush_control.nr_pdflush_threads);
```

11. 原子递增ticks

```c
atomic_inc_return(&ticks);
```

12. 线程信号handler的引用数的增加和减少

```c
atomic_inc_return(&t->sig->ref);
if (t->sig) {
    // !!! for shared
    int ref = atomic_dec_return(&t->sig->ref) - 1;
    if (ref == 0) {
        kfree((void *)t->sig);
    }
    t->sig = NULL;
}
```



我们还需要一些原子的（可以不是原子的）bit 操作来实现基数树，于是我们添加了下面这些指令：

```c
#define __WORDSIZE 64
#define BITS_PER_LONG __WORDSIZE
#define BIT_WORD(nr) ((nr) / BITS_PER_LONG)
#define BIT_MASK(nr) (1UL << ((nr) % BITS_PER_LONG))
#if (BITS_PER_LONG == 64)
#define __AMO(op) "amo" #op ".d"
#elif (BITS_PER_LONG == 32)
#define __AMO(op) "amo" #op ".w"
#else
#error "Unexpected BITS_PER_LONG"
#endif

#define __op_bit_ord(op, mod, nr, addr, ord) \的的的的
    __asm__ __volatile__(                    \
        __AMO(op) #ord " zero, %1, %0"       \
        : "+A"(addr[BIT_WORD(nr)])           \
        : "r"(mod(BIT_MASK(nr)))             \
        : "memory");

#define __op_bit(op, mod, nr, addr) \
    __op_bit_ord(op, mod, nr, addr, )
#define __NOP(x) (x)
#define __NOT(x) (~(x))
static inline void set_bit(int nr, volatile uint64 *addr) {
    __op_bit(or, __NOP, nr, addr);
}

static inline void clear_bit(int nr, volatile uint64 *addr) {
    __op_bit(and, __NOT, nr, addr);
}

static inline int test_bit(int nr, const volatile void *addr) {
    return (1UL & (((const int *)addr)[nr >> 5] >> (nr & 31))) != 0UL;
}
```

基数树中使用的函数接口如下：

```c
static inline void tag_set(struct radix_tree_node *node, uint32 tag,
                           int offset) {
    set_bit(offset, node->tags[tag]);
}

static inline void tag_clear(struct radix_tree_node *node, uint32 tag,
                             int offset) {
    clear_bit(offset, node->tags[tag]);
}

static inline int tag_get(struct radix_tree_node *node, uint32 tag,
                          int offset) {
    return test_bit(offset, node->tags[tag]);
}
```







