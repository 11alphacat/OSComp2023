# VFS(虚拟文件系统)

## 概述

我们为 FAT32 文件系统的 api 封装了一层抽象，引入了 inode 节点，file 数据结构，使得在系统调用层面屏蔽了具体的文件系统类型。

```c
uint64 sys_read(void) {
    struct file *f;
    int count;
    uint64 buf;

	...

    return f->f_op->read(f, buf, count);
}
```

在上面这个例子中，f 指针最后调用的句柄被动态绑定到 fat32_fileread()，从函数的命名很容易得知这是个 FAT32 的 api。

这样对具体的文件系统进行抽象的意义，是为了让应用程序在执行诸如 cp 命令时，对像“将一个格式化为 FAT32 的U盘中 file.txt 文件拷贝到 ext2 格式化的磁盘中”这样的行为习以为常。

如果内核不提供这样的抽象，那么用户很可能需要明确地认识自己要处理的文件系统类型，再分别调用 libc 库中相应文件系统的函数来读写磁盘，这不会给程序员的日常工作带来幸福感。

而现在用户可以使用统一的 read 和 write 系统调用来处理磁盘乃至内存中的资源，使用大家都很熟悉的文件描述符作为句柄，这一切都要感恩 UNIX 哲学中 “Everything is a file ”的理念。  


## 三个对象

我们没有完全实现 Linux VFS 中的那四个对象，而只使用了 file、inode 和 superblock 对象。它们的结构体定义如下，可以在 fs/vfs/fs.h 目录中找到。

### **FILE**

```C
struct file {
    type_t f_type;
    ushort f_mode;
    uint32 f_pos;
    ushort f_flags;
    ushort f_count;
    short f_major;

    int f_owner; /* pid or -pgrp where SIGIO should be sent */
    union file_type f_tp;
    const struct file_operations *f_op; 
    unsigned long f_version;
};
```

- ​f_type 字段是个枚举类型，包含 FD_DEVICE、FD_PIPE、FD_INODE 等，表示文件类型。
- ​f_pos 字段是文件的读写指针位置，刚打开一个文件时会被设置为 0。
- ​f_flags 字段记录了文件的读写执行权限。
- ​f_count 字段记录了文件的引用次数，关闭一个文件会让该字段值减 1。当该字段变为 0 时，表示该 file 结构体在内- 存中的全局打开文件表项可以被重新使用。
- ​f_tp 字段是表示该 file 指向的具体文件。它可能是一个 inode 节点指针，表示磁盘中持久化的一个文件；也可能是存在于内存中的管道文件。
- f_op 字段是一个 file 对象能进行操作的集合，包含基本的 read、write，获取文件信息的 fstat，以及对设备操作的 ioctl。我们有意减少该集合支持的操作数量，力求得到最小的操作集。


### **INODE**

```c
struct inode {
    uint8 i_dev;   // note: 未在磁盘中存储
    uint32 i_ino;  // 对任意给定的文件系统的唯一编号标识：由具体文件系统解释
    uint16 i_mode; // 访问权限和所有权
    int ref;       // Reference count
    int valid;
    
    uint16 i_nlink; // Note: FAT32 does not support 
                    // hard link, reserve for vfs interface
    uint i_uid;
    uint i_gid;
    uint16 i_rdev; // major、minor, 8 + 8
    uint32 i_size;
    uint16 i_type;

    uint64 i_blksize; // bytes of one block
    uint64 i_blocks;  // numbers of blocks
    struct semaphore i_sem;  // binary semaphore 

    const struct inode_operations *i_op;
    struct _superblock *i_sb;
    struct inode *i_mount;
    struct inode *parent;

    fs_t fs_type;

    struct hash_table *i_hash;  // O(1) to get inode information
    int off_hint;  // speed up dirlookup

    union {
        struct fat32_inode_info fat32_i;
    };  

};
```

inode 结构体的内容比较多，这里选择几个介绍
- i_dev 表示设备号，表示存储的块设备，在 Buffer cache 层传递给 bread 函数来读取某个设备。
- i_ino 是一个编号标识，由具体的文件系统负责解释。
- ref 是引用计数，只有当引用数为 1 时，该文件才可能在磁盘中被真正删除。ref 为 0 时，意味着内存中的全局 inode 表项字段可以回收。
- i_rdev 表示设备的主副设备号，我们将末 8 位作为副设备号，往前 8 位作为主设备号。								
- i_size 字段表示该 inode 的文件在磁盘中所占的大小，单位是字节。
- i_op 字段一个 inode 对象能进行操作的集合。

请注意，inode 是对磁盘中所有持久化文件的抽象，而 file 则是一个用户进程眼中对系统资源的认识，并借助文件描述符以类似文件的方式来访问系统中的资源。

### **SUPERBLOCK**

```c
struct _superblock {
    struct semaphore sem; /* binary semaphore */
    uint8 s_dev;          // device number

    uint32 s_blocksize;       // 逻辑块的数量
    uint32 sectors_per_block; // 每个逻辑块的扇区个数
    uint cluster_size;        // size of a cluster

    // uint32 s_blocksize_bits;
    uint n_sectors;   // Number of sectors
    uint sector_size; // size of a sector

    struct super_operations *s_op;
    struct inode *s_mount;
    struct inode *root;

    union {
        struct fat32_sb_info fat32_sb_info;
        // struct xv6fs_sb_info xv6fs_sb;
    };
};
```

超级块对象是一个文件系统信息的集合体，一个文件系统应当在内存中存在一个超级块对象。  

## 两个操作集合
可以到 fs/vfs/ops.h 中查看虚拟文件系统的分层接口，到 fs/vfs/ops.c 查看相应的实现。
### **INODE_OPERATIONS**

```c
struct inode_operations {
    void (*iunlock_put)(struct inode *self);
    void (*iunlock)(struct inode *self);
    void (*iput)(struct inode *self);
    void (*ilock)(struct inode *self);
    void (*iupdate)(struct inode *self);
    struct inode *(*idup)(struct inode *self);
    void (*ipathquery)(struct inode *self, char *kbuf);
    ssize_t (*iread)(struct inode *self, int user_src, uint64 src, uint off, uint n);
    ssize_t (*iwrite)(struct inode *self, int user_dst, uint64 dst, uint off, uint n);

    // for directory inode
    struct inode *(*idirlookup)(struct inode *dself, const char *name, uint *poff);
    int (*idempty)(struct inode *dself);
    ssize_t (*igetdents)(struct inode *dself, char *buf, size_t len);
    struct inode *(*icreate)(struct inode *dself, const char *name, uchar type, short major, short minor);
    int (*ientrycopy)(struct inode *dself, const char *name, struct inode *ip);
    int (*ientrydelete)(struct inode *dself, struct inode *ip);
};
```

### **FILE_OPERATIONS**

​		相较于 inode_opearations，我们设计的 file_operations 结构体要简洁得多。

```c
struct file_operations {
    struct file *(*dup)(struct file *self);
    ssize_t (*read)(struct file *self, uint64 __user dst, int n);
    ssize_t (*write)(struct file *self, uint64 __user src, int n);
    int (*fstat)(struct file *self, uint64 __user dst);
    long (*ioctl)(struct file *self, unsigned int cmd, unsigned long arg);
};
```

