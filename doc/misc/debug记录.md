## misaligned pagefault

- 在支持superpage的过程中，进程触发了一个pagefault，导致该进程被杀死：

  该进程的页表打印出来如下：

  <img src="../image/debug记录.assets/superpage.png"  style="zoom:67%;" />

  该进程被kill时，对应的scause和stval如下图：

  ![image-20230604215228062](../image/debug记录.assets/trap.png)

  可见该进程是在访问0x2800000这个地址时触发了一个STORE PAGEFAULT

- 问题排查：

  首先从页表中可见的是，2800000这个地址位于该进程的页表中，且拥有PTE_W权限，此外，我们排除了常见的pagefault原因，诸如访问空指针等错误

  在经过了长时间的debug后，我们发现这个pagefault的原因是地址没有对齐导致的

  根据riscv-privileged手册中的描述：

  ```
  If i > 0 and pte.ppn[i − 1 : 0] ̸= 0, this is a misaligned superpage; stop and raise a page-fault
  exception corresponding to the original access type.
  ```

- 原因:伙伴系统实现中，多核的伙伴系统采用page_to_offset的机制而非page_to_pa

  即每个伙伴系统看做一个相对的地址空间，该地址空间的范围是：从0到这个伙伴系统的内存的ending

  而这个相对空间的起始地址对应的绝对物理地址可能没有按照大页的方式对齐，导致这里出现地址不对齐的page fault

- 解决方式：

  通过调整内存布局的宏的大小，让每个CPU拥有的物理地址空间的起始地址都是2M对齐的




## 合并过程中保留了两份不同的结构体定义

在对不同分支的代码进行合并时，由于我的疏忽，在两个不同的头文件中保留了一个结构体的两份不同的定义

``` c++
struct proc {

}; // in header_file_1
struct proc {

}; // in header_file_2

/* src1 */
#include"header_file_1"
/* use struct proc */

/* src2 */
#include"header_file_2"
/* use struct proc */
```

具体的情形如上，在两个不同的头文件中都保留了proc这个结构体的定义，但是这两个结构体中的字段位置甚至数量都有所不同，且在两个不同的源文件中，分别引用了这两个头文件，这导致两处不同的代码使用不同的偏移访问proc中的同一个字段，导致panic的发生

即便是开了`-Wall -Werror`, 编译器也对这类"错误"熟视无睹!

## FAT32文件系统
FAT32文件系统的 fcb 用以记录的关于文件类型的字段只有 **DIR_Attr**，用以标识是否为目录。  
在初始化镜像时，Linux 系统下的 cp 工具能够正确地往该字段写入值。  
然而，我们重命名了 fcb 中的保留字段 **DIR_Dev** 用以保存更多的文件类型信息，以及主副设备号。这  
与官方的行为不一致，因此需要我们更多留心。  
此外，大多 FAT32 的 API 在涉及文件类型的变量时，使用的多为 uchar 类型，然而与 Linux 兼容  
的 inode 的节点中的 i_mode 字段是 uint16 类型。因此我们规定，如果 API 中的类型变量是  
uchar 类型，那么按照标准的 FAT32 来处理，若为 uint16， 则需要做更多的转换或特判。  
然而危险的是，如果错误地将 uint16 类型的变量传给了接收 uchar 类型的函数，编译器不会报错，  
即便我们打开了 `-Wall -Werror`！  
涉及到的 API 有 fat32_fcb_init, fat32_inode_alloc 等  
此外，FAT32 的 fcb 也不支持记录文件的使用权限。因此如果涉及到处理文件权限的情况，  
如下处理：
- 用户传入的 mode 参数，只取类型，忽略权限信息；  
- 传给用户的权限信息，全赋值为 0777

### 设备相关
在 fat32_inode_load_from_disk 函数中，有如下代码段：
```c
/* 有 bug 的代码 */ 
ip->i_rdev = FATDEV_TO_IRDEV(dirent_s_tmp->DIR_Dev);
mode = FATDEV_TO_ITYPE(dirent_s_tmp->DIR_Dev);  
if (ip->i_rdev) {       // 尝试用设备号字段判断类型，bug !
    // a device file
    ip->i_size = 0;
    ip->i_mode = mode;
} else if (DIR_BOOL(ip->fat32_i.Attr)) {
    // a directory
    ip->i_mode = S_IFDIR;
    ip->i_size = DIRLENGTH(ip);
} else {            // 暂不支持 FIFO 和 SOCKET 文件
    ip->i_mode = S_IFREG;
    ip->i_size = ip->fat32_i.DIR_FileSize;
}
...
```
上面这段代码的目的是解析磁盘中读取 fcb 中的 DIR_Dev 字段，转换为 inode 文件的 mode 字段和  
i_rdev 字段，前者记录文件类型，后者记录设备文件的主副设备号。在条件判断中，使用了 ip->i_rdev  
作为条件，如果不为0，则该文件是设备文件。  
这样看似正确，然而在我们的系统中，当使用 mknod 系统调用创立一个新的设备文件（如 CONSOLE,TTY,etc）  
时，系统无法将设备文件类型同设备号一起写入磁盘，而是先创建设备文件的 fcb，然后加载到内存的 inode 中， 
在内存中为 inode 赋予主副设备号，再调用 iupdate 函数将设备号的信息更新到磁盘中。  
所以，如果按上面代码的判断条件，在处理第一次创建的设备文件时，执行到判断条件时，ip 并不具有设备号  
的信息，这会导致进入错误的判断分支，甚至会导致系统无法运行 init 进程。  
因此正确的做法应当为
```c
ip->i_rdev = FATDEV_TO_IRDEV(dirent_s_tmp->DIR_Dev);
mode = FATDEV_TO_ITYPE(dirent_s_tmp->DIR_Dev);  
if (S_ISCHR(mode) || S_ISBLK(mode) ) {   // 用类型判断，而非用设备号！
    // a device file
    ip->i_size = 0;
    ip->i_mode = mode;
} else if (DIR_BOOL(ip->fat32_i.Attr)) {
    // a directory
    ip->i_mode = S_IFDIR;
    ip->i_size = DIRLENGTH(ip);
} else {            // 暂不支持 FIFO 和 SOCKET 文件
    ip->i_mode = S_IFREG;
    ip->i_size = ip->fat32_i.DIR_FileSize;
}
...
```
