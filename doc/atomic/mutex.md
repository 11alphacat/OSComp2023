

#### 总结一下同步和互斥语句的使用顺序：

- 简单计数器用原子指令 AMO。
- 多指令临界区用自旋锁 spinlock。
- 需要broadcast用条件变量 cond。
- 一般的内核休眠锁和同步语句用 semaphore。
- 用户级线程pthread的同步用 futex。



