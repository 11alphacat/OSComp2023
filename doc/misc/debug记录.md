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