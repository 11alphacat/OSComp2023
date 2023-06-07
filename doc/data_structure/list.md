# 内核通用数据结构——list

#####      如何使用这个list.h轮子？

1. 定义一个链表的结构体

```c
// 链表数据结构
struct list_head {
    struct list_head *next, *prev;
};
typedef struct list_head list_head_t;
```

2. 通过list_head得到嵌套的结构体指针

```c
#ifndef container_of
#define container_of(ptr, type, member) ({                      \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) ); })
#endif

#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)
```

3. 初始化list_head结构体

```c
static inline void INIT_LIST_HEAD(struct list_head *list) {
    list->next = list;
    list->prev = list;
}
```

4. 使用一些通用的接口，下面是内核中经常使用的一些接口

```c
// 1. delete it and re init
static inline void list_del_reinit(struct list_head *entry);

// 2. add a pnew entry
static inline void list_add_tail(struct list_head *pnew, struct list_head *head);

// 3. tests whether a list is empty
static inline int list_empty(const struct list_head *head);

// 4. get the first element from a list
#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->next, type, member)

// 5. iterate over list of given type
#define list_for_each_entry(pos, head, member)               \
    for (pos = list_first_entry(head, typeof(*pos), member); \
         &pos->member != (head);                             \
         pos = list_next_entry(pos, member))

// 6. iterate over list of given type safe against removal of list entry
#define list_for_each_entry_safe(pos, n, head, member)       \
    for (pos = list_first_entry(head, typeof(*pos), member), \
        n = list_next_entry(pos, member);                    \
         &pos->member != (head);                             \
         pos = n, n = list_next_entry(n, member))

// 7. iterate over list of given type safe against removal of list entry
// given first head
#define list_for_each_entry_safe_given_first(pos, n, head_f, member, flag) \
    for (pos = head_f,                                                     \
        n = list_next_entry(pos, member);                                  \
         flag || &pos->member != (&head_f->member);                        \
         pos = n, n = list_next_entry(n, member), flag = 0)
```



##### 下面是内核中使用了list.h的例子

1. 使用list.h实现的queue.h通用数据接口

```c
struct Queue {
    struct spinlock lock;
    struct list_head list;
    char name[12];
    enum queue_type type;
};
typedef struct Queue Queue_t;
```

2. 使用list.h实现的 LRU bcache

```c
struct {
    struct spinlock lock;
    struct buffer_head buf[NBUF];

    list_head_t head;
} bcache;
```

3. 使用list.h实现的hash table

```c
struct hash_node {
    union {
        int key_id;
        void *key_p;
        // char key_name[NAME_LONG_MAX];
        char key_name[NAME_LONG_MAX / 4];
    };
    void *value; // value
    struct list_head list;
};

struct hash_entry {
    struct list_head list;
};
```

4. 使用list.h实现的buddy system

```c
struct page {
    // use for buddy system
    int allocated;
    int order;
    struct list_head list;

    // use for copy-on-write
    struct spinlock lock;
    int count;
};
struct free_list {
    struct list_head lists;
    int num;
};
struct phys_mem_pool {
    uint64 start_addr;
    uint64 mem_size;
    /*
     * The start virtual address (for used in kernel) of
     * the metadata area of this pool.
     */
    struct page *page_metadata;

    struct spinlock lock;

    /* The free list of different free-memory-chunk orders. */
    struct free_list freelists[BUDDY_MAX_ORDER + 1];
};
```

5. 使用list.h实现的 virtual memory area

```c
struct vma {
    enum {
        VMA_MAP_FILE,
        VMA_MAP_ANON, /* anonymous */
    } type;
    struct list_head node;
    vaddr_t startva;
    size_t size;
    uint32 perm;

    // temp
    int used;

    int fd;
    uint64 offset;
    struct file *fp;
};
```

6. 使用list.h维护父子进程之间的关系链表

```c
struct list_head sibling_list; // its sibling
```

7. 使用list.h维护的进程状态队列

```c
struct list_head state_list; // its state queue
```

8. 使用list.h维护的线程组链表

```c#
struct thread_group {
    spinlock_t lock; // spinlock
    tid_t tgid;      // thread group id
    int thread_idx;
    int thread_cnt;           // the count of threads
    struct list_head threads; // thread group
    struct tcb *group_leader; // its proc thread group leader
};
```

9. 使用list.h维护的线程等待队列

```c
struct list_head wait_list; // waiting queue (semaphore)
```

10. 使用list.h维护的线程状态队列

```c
struct list_head state_list; // its state queue
```

11. 使用list.h维护的信号处理队列

```c
// pending signal queue head of proc
struct sigpending {
    struct list_head list;
    sigset_t signal;
};

// signal queue struct
struct sigqueue {
    struct list_head list;
    int flags;
    siginfo_t info;
};
```



##### 需要主要的点：

1. 对于一些临界的全局变量，一定要加锁保护！不要忽视在进行链表操作时进行加锁保护！
2. 遍历链表，最好使用带safe后缀的接口！
3. 所有的链表都是带头结点的链表，所以在定义数据结构的时候一定要单独定义一个链表的head entry。但是对于进程的父子关系的维护，我们自己改写了list.h轮子，可以实现不带头结点的链表的安全遍历。