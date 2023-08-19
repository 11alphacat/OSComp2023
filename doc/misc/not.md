

系统初始化内存消耗？
. 内核页表kpgtbl
. kvmmap 进行内存映射
. 给每个线程分配内核栈
. tid_map、pid_map和futex_map三个全局hash表的初始化
. 内存中bit_map和fat_table的初始化

大内存分配？
busybox会4次调用sendfile，每次都是需要分配4096个page


所有可能用到 kzalloc、 kmalloc 或者 kmalloc 的地方：

1. inode hash 表初始化
   dp->i_hash = (struct hash_table *)kmalloc(sizeof(struct hash_table));
2. inode hash 的value 结点初始化
   struct inode_cache *c = (struct inode_cache *)kmalloc(sizeof(struct inode_cache));
3. long entry 栈初始化
   stack->data = (elemtype *)kmalloc(30 * 32);
4. ipc 的hash表初始化
   ids->key_ht = (struct hash_table *)kmalloc(sizeof(struct hash_table));
5. readv 的 缓冲区初始化 
   if ((kbuf = kmalloc(totsz)) == 0) {
    goto bad;
   }
6. hash table inode 的初始化
   node_new = (struct hash_node *)kmalloc(sizeof(struct hash_node));
7. sbuf 的初始化
   sp->buf = (char *)kmalloc(sizeof(char) * n);
8. cow 的大页分配 
   if ((mem = kmalloc(SUPERPGSIZE)) == 0) {
    return -1;
   }
9. cow 的普通页分配
   if ((mem = kmalloc(PGSIZE)) == 0) {
    return -1;
   }
10. 分配elf头结构体
    Elf64_Ehdr *elf_ex = kmalloc(sizeof(Elf64_Ehdr));
11. elf data
    elf_phdata = kmalloc(size);
12. paddr_t pa = (paddr_t)kmalloc(PGSIZE);
13. 线程内核栈的分配
    char *pa = kmalloc(KSTACK_PAGE * PGSIZE);
14. futex结构体的分配
    struct futex *fp = (struct futex *)kzalloc(sizeof(struct futex));
15. 页缓存 page 分配
    pa = (uint64)kzalloc(PGSIZE * (end_idx - start_idx))
16. 页缓存 page item 分配 （读磁盘）
    p_item = (struct Page_item *)kzalloc(sizeof(struct Page_item))
17. 页缓存 page item 分配 （写磁盘）
    p_item = (struct Page_item *)kzalloc(sizeof(struct Page_item))
18. bit map 分配 和 内存FAT 表的分配
    idx_page = (uint64)kzalloc(PGSIZE * n)
19. fcb 字符数组缓存分配, copy
    uchar *fcb_char = kzalloc(fcb_char_len);
20. fcb 字符数组缓存分配，create
    uchar *fcb_char = kzalloc(FCB_MAX_LENGTH);
21. bio vec 的分配
    vec_cur = (struct bio_vec *)kzalloc(sizeof(struct bio_vec))
22. i_mapping 的分配
    struct address_space *mapping = kzalloc(sizeof(struct address_space));
23. 目录 travel 的缓冲区分配
    (kbuf = kzalloc(sz)) == 0
24. shared memory 结构体分配
    shp = kzalloc(sizeof(struct shmid_kernel))
25. send_file 缓冲区分配
    kbuf = kzalloc(count)
26. getdents 缓冲区分配
    kbuf = kzalloc(sz)
27. writev 缓冲区分配
    kbuf = kzalloc(totsz)
28. shared memory data
    sfd = (struct shm_file_data *)kzalloc(sizeof(struct shm_file_data))
29. hash表的entry分配
    table->hash_head = (struct hash_entry *)kzalloc(table->size * sizeof(struct hash_entry));
30. radix tree 的结点分配
    ret = (struct radix_tree_node *)kzalloc(sizeof(struct radix_tree_node));
31. mm 结构体分配
    mm = kzalloc(sizeof(struct mm_struct));
32. 内核页表分配
    pagetable_t kpgtbl = (pagetable_t)kzalloc(PGSIZE);
33. 三级页表的分配
    pagetable = (pde_t *)kzalloc(PGSIZE)
34. 用户页表的分配
    pagetable = (pagetable_t)kzalloc(PGSIZE);
35. uvmalloc 的物理页分配
    mem = kzalloc(PGSIZE);
    mem = kzalloc(SUPERPGSIZE);     
    mem = kzalloc(PGSIZE);  
36. uvmcopy 的物理页分配
    paddr_t new = (paddr_t)kzalloc(PGSIZE);
37. uvm_thread_stack 线程栈的分配
    paddr_t pa = (paddr_t)kzalloc(PGSIZE);
38. uvm_thread_trapframe 线程trapframe的分配
    paddr_t pa = (paddr_t)kzalloc(PGSIZE);
39. load_elf_binary
    void *pa = kzalloc(PGSIZE);
40. uvminit
    mem = kzalloc(PGSIZE);  
41. 信号的处理handler 分配
    t->sig = (struct sighand *)kzalloc(sizeof(struct sighand))
42. root inode 的分配
    struct inode *root_ip = (struct inode *)kalloc();
43. 管道的分配
    pi = (struct pipe *)kalloc();
44. 信号处理队列结点的分配
    q = (struct sigqueue *)kalloc()
45. 线程组的分配
    p->tg = (struct thread_group *)kalloc()
46. ipc namespace 的分配
    p->ipc_ns = (struct ipc_namespace *)kalloc()