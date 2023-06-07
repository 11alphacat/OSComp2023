# 兼容Xv6的Buffer cache

数据结构就是一个用链表连接的磁盘块缓存。使用了LRU算法进行移动。

主要的接口：

1. **初始化buffer cache**

```c
void binit(void);
```

- 用list.h的接口完成buffer cache的链表初始化。
- 同时完成每个buffer_head的互斥信号量和同步信号量的初始化。

2. **遍历整个buffer cache，查找是否有扇区号的缓冲。**

```c
static struct buffer_head* bget(uint dev, uint blockno);
```

- 从前向后找是否有已经缓存的buffer_head。
- 如果没有，那么就需要从后向前找一个refcnt为0的buffer_head。

- 分别使用list_for_each_entry和list_for_each_entry_reverse进行遍历。
- 返回前，需要用sema_wait获取buffer_head的互斥信号量。

3. **读取磁盘内容到buffer_head中。**

```c
struct buffer_head* bread(uint dev, uint blockno);
```

- 用bget获取buffer_head。如果valid为0，就需要从磁盘中读取数据。

4. **将内存中的缓存写回磁盘。**

```c
void bwrite(struct buffer_head *b);
```

- 设置dirty为1即可。

5. **释放buffer_head的锁。**

```c
void brelse(struct buffer_head *b);
```

- 如果dirty为1，就需要写回磁盘。
- 及时释放锁。
- 将buffer_head移动到bcache的头部。





#### 总结：

- 每次需要读取扇区的内容时，给定扇区号，用bread读取即可。
- 读取完扇区内容后，用brelse释放锁即可。
- 如果需要更新扇区的内容，用bwrite写回即可。

