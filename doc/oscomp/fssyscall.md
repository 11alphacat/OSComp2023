# 支持的文件系统调用


#### **#define SYS_getcwd 17**

- 功能：获取当前工作目录。

- 实现：调用 <span style="color: purple;">ip->i_op->igetdents</span>。

#### **#define SYS_pipe2 59**

- 功能：创建管道。

- 实现： 
1. 调用 <span style="color: purple;">pipealloc</span>
 分配两个管道文件；  
2. 给当前进程分配两个文件描述符来代指那两个管道文件; 
3. 将文件描述符拷贝到用户空间。

#### **#define SYS_dup 23**

- 功能：复制文件描述符。
- 实现：调用 <span style="color: purple;">f->f_op->dup</span>。

#### **#define SYS_dup3 24**

- 功能：复制文件描述符，并指定了新的文件描述符。

- 实现：调用 <span style="color: purple;">assist_setfd</span>，如果新的文件描述符已经打开，  
则会关闭该文件。然后使用新文件描述符绑定文件  
（注意：在多线程环境关闭和重用打开文件表项应做到原子性）。  
最后调用 <span style="color: purple;">f->f_op->dup</span> 将文件引用计数增加。

#### **#define SYS_chdir 49**

- 功能：切换工作目录。

- 实现：调用 <span style="color: purple;">namei</span>
 查找文件的 inode，然后将进程  
当前目录修改为该 inode。

#### **#define SYS_openat 56**

- 功能：打开或创建一个文件。

- 实现：如果传入了 `O_CREAT`，则调用 <span style="color: purple;">assist_create</span>，  
该函数最后调用 <span style="color: purple;">dp->i_op->icreate</span>
 尝试创建一个 inode  
（如果已经存在则相当于做查找）；否则，调用 <span style="color: purple;">find_inode</span>
  
进行查找。然后为 inode 文件分配一个 file 结构体并填充，  
为进程分配一个文件描述符，最后返回该文件描述符。


#### **#define SYS_close 57**

- 功能：关闭一个文件描述符。

- 实现：调用 <span style="color: purple;">generic_fileclose</span>。

#### **#define SYS_getdents64 61**

- 功能：获取目录的条目。

- 实现：动态申请一个空间作为缓冲区，调用 <span style="color: purple;">ip->i_op  
\->igetdents</span> 将该目录中所有条目信息填充到该缓冲区，  
再调用 <span style="color: purple;">either_copyout</span> 将缓存区的内容拷贝到用户空间，  
最后归还申请的空间。

#### **#define SYS_read 63**

- 功能：从一个文件描述符中读取。

- 实现：调用 <span style="color: purple;">f->f_op->read</span>。

#### **#define SYS_write 64**

- 功能：从一个文件描述符中写入。

- 实现：调用 <span style="color: purple;">f->f_op->write</span>。


#### **#define SYS_unlinkat 35**

- 功能：移除指定文件的链接(可用于删除文件)；

- 实现：调用 <span style="color: purple;">do_unlinkat</span>，该函数会调用 <span style="color: purple;">dp->i_op  
\->ientrydelete</span> 删除父目录中的该文件的 fcb，然后  
调用 <span style="color: purple;">dp->i_op->iunlock_put</span>，该操作会导致文件的  
磁盘数据被删除。

#### **#define SYS_mkdirat 34**

- 功能：创建目录。

- 实现：调用 <span style="color: purple;">assist_icreate</span>，创建一个目录类型的文件。

#### **#define SYS_fstat 80**

- 功能：获取文件状态；
- 实现：调用 <span style="color: purple;">f->f_op->fstat</span>。