## inode 层的接口重新设计和改进

1. **首先就是sys_read 和 sys_write 的底层接口需要统一。**

只要是读文件和写文件（目录文件或者是数据文件），都会调用 fat32_inode_read 和 fat32_inode_write。

由于目录文件和数据文件都在磁盘的data region区域，我们选择在底层上不区分这两种文件类型，都看作是一个inode+数据块的形式，文件的内容无论是long entry 或者是 short entry，都看作是字节组成的数组。

于是我们统一用Linux 页缓存来进行加速读写。

fat32_inode_read 调用 do_generic_file_read，而 fat32_inode_write 调用 do_generic_file_write。





2. **然后是FAT表的分配、修改和释放。**

如果每次都通过bread、bwrite 和 brelse 来读取FAT表、修改FAT表，会导致磁盘I/O次巨大（尤其对于iozone这样的测试）。于是我们设计内存的bitmap、全局的FAT表缓存、局部的inode混合索引来加速随机读写、顺序读写的遍历磁盘的操作，减少无谓读磁盘的操作。

内存bitmap接口如下：

```c
// alloc a valid cluster given bit map
FAT_entry_t fat32_bitmap_alloc(struct _superblock *sb, FAT_entry_t hint);
// set/clear bit map given cluster number
void fat32_bitmap_op(struct _superblock *sb, FAT_entry_t cluster, int set);
```

全局的FAT表缓存接口如下：

```c
// set fat table entry given cluster number using fat table in memory
void fat32_fat_cache_set(FAT_entry_t cluster, FAT_entry_t value);
// get fat table entry given cluster number using fat table in memory
FAT_entry_t fat32_fat_cache_get(FAT_entry_t cluster);
```

内存inode混合索引接口如下：

```c
// lookup or create index table to find the cluster_num of logistic_num
uint32 fat32_ctl_index_table(struct inode *ip, uint32 l_num, uint32 cluster_num);
// free index table
void fat32_free_index_table(struct inode *ip);
```

为了使分配只需要遍历一次，我们用hint线索实现了从FAT表头部到尾部，再到头部这样的循环分配，大部分情况是只需要查找1次，就可以找到空闲的cluster！



3. **其次是查找目录文件下的指定名称的文件需要加速。**

为了减少无谓的遍历，我们需要尽量让遍历目录变得更加简洁，减少从头开始遍历的次数。我们使用hash表+hint（线索）的方式实现快速查找指定目录下的文件，同时代替了Linux下的dentry的功能。

接口如下：

```c
// lookup inode given its name and inode of its parent
struct inode *fat32_inode_dirlookup(struct inode *dp, const char *name, uint *poff);
// dirlookup with hint
struct inode *fat32_inode_dirlookup_with_hint(struct inode *dp, const char *name, uint *poff);
```

inode专用hash表如下：

```c
// init the hash table of inode
void fat32_inode_hash_init(struct inode *dp);
// insert inode hash table
int fat32_inode_hash_insert(struct inode *dp, const char *name, struct inode *ip, uint off);
// lookup the hash table of inode
struct inode *fat32_inode_hash_lookup(struct inode *dp, const char *name);
// delete hash table of inode
void fat32_inode_hash_destroy(struct inode *dp);
```



4. **最后是目录遍历的统一接口。**

类似list.h 的思路，我们需要实现一个目录遍历过程抽象的接口，方便编码和调试。我们将目录遍历和页缓存结合，每次目录遍历都是使用一个统一接口+特定简短handler来实现特定功能。

接口如下：

```c
// general_travel
void fat32_inode_general_trav(struct inode *dp, struct trav_control *tc, trav_handler fn);
// for dirlookup and getdents
void fat32_inode_travel_fcb_handler(struct trav_control *tc);
// for dirlookup
void fat32_inode_dirlookup_handler(struct trav_control *tc);
// for getdents
void fat32_inode_getdents_handler(struct trav_control *tc);
// for fat32_dir_fcb_insert_offset
void fat32_dir_fcb_insert_offset_handler(struct trav_control *tc);
// for fat32_isdirempty
void fat32_isdirempty_handler(struct trav_control *tc);
// for fat32_find_same_name_cnt
void fat32_find_same_name_cnt_handler(struct trav_control *tc);
```

为了实现一个通过遍历目录的功能函数，只需要完成下面三个步骤：

1. 传递参数。
2. 编写简短的handler接口，并用fat32_inode_general_trav调用这个接口。
3. return 返回值即可。



**总结一下改进思路：**

1. 统一文件 read 和 write 的接口，使用Linux的基于radix tree 的页高速缓存。
2. 使用 hint 和 hash 加速文件查找。
3. 使用 hint 和 bitmap 加速free cluster 的分配。
4. 使用 bitmap 和 FAT表内存缓存加速FAT表的修改。
5. 使用 统一的目录遍历接口方便调试，减少代码的耦合。





