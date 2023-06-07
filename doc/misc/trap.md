### 初始化

在系统启动的时候，0号CPU负责进行trap的初始化。

```c
void trapinit(void);
void trapinithart(void);
```

需要将stvec中写入kernelvec的地址，内核中发生异常时将跳转到这个寄存器所保存的地址。并启动设置sie寄存器的SEIE、STIE和SSIE字段来启动S态的外部中断、时钟中断和软件中断。并使用sbi_legacy_set_timer来设置定时器。

```c
w_stvec((uint64)kernelvec);
w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);
SET_TIMER();
```

还需要初始化PLIC来处理设备中断。

```c
void plicinit(void);
void plicinithart(void);
```

设置可以响应UART和VIRTIO的中断即可。





### 用户态发生异常的入口地址初始化

但用alloc_thread分配了一个新的线程后，会将其context的ra寄存器修改为(uint64)thread_forkret，当这个线程变为TCB_RUNNABLE时，会在第一次被调度器调度的时候跳转到thread_forkret，调用thread_usertrapret设置在用户态发生异常时的跳转地址为uservec。

```c
uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
w_stvec(trampoline_uservec);
```

并保存user trap的处理函数地址用于下次从用户态切到内核态的trap的处理函数。

```c
t->trapframe->kernel_trap = (uint64)thread_usertrap;
```





### 当用户态程序发生了trap

由于线程在thread_usertrapret中设置了stvec 寄存器内容为 uservec的地址，所以会跳转到uservec中。

```c
.section trampsec
.globl trampoline
trampoline:
.align 4
.globl uservec
uservec:  
	...
```

在uservec中，虽然时S态，但是使用的是U态的页表。用户空间和内核空间都映射了TRAMPOLINE这个页，所以陷入内核时不需要切换页表。

首先需要将a0寄存器存入sscratch寄存器。然后用a0寄存器读取TRAPFRAME。需要将保存在TRAPFRAME中的寄存器全部加载。

然后用sscratch和t0寄存器作为中间寄存器，保存a0寄存器的内容到trapframe->a0中。

然后就是加载sp、tp寄存器。

用t0和t1寄存器读取thread_usertrap的地址和内核页表地址satp。

t1的内容加载进入satp寄存器。

利用jr指令跳转到thread_usertrap。

**TODO：决赛我们将修改这段汇编，将sscratch寄存器腾出来保存CPU 的id，tp寄存器保存线程的tidx。**





### **执行完uservec后会跳转到thread_usertrap**

当用户态发生了异常后，会跳转到uservec陷入内核，然后执行thread_usertrap。

```c
void thread_usertrap(void);
```

下面是usertrap的处理流程：

首先需要通过sstatus寄存器确保发生异常的来源为用户态。

然后设置stvec寄存器kernelvec的地址，主要原因是当用户程序陷入内核时已经处于了内核态，此时发生的异常需要跳转到kernelvec进行处理。

由于本次发生异常可能需要再次返回用户空间继续执行用户态代码，所以本次跳转需要保存程序执行流的上下文。所有线程都设置了trapframe用来保存发生trap之前的上下文信息。包括epc、ra、sp等RISCV 32个寄存器。还有内核页表、内核栈顶指针、usertrap的处理指针和CPU id。

需要读取sepc寄存器保存用户态的program counter。

通过读取scause寄存器的内容，判断用户态发生本次异常的原因：

1. **SYSCALL 表示 通过ecall 系统调用，scause 为 8**

![syscall](../image/trap.assets/syscall.png)


2. **scause 最高位为1，低8为 9，就是外部中断。**

![external](../image/trap.assets/external.png)

外部中断需要设置uart和virtio即可。

3. **scause 最高位为1，低8位为9，就是时钟中断。**

![timer](../image/trap.assets/timer.png)

内核中的ticks变量只有0号cpu才进行设置（即clockintr只有0号CPU发生时钟中断时才会调用），其他cpu通过SET_TIMER设置是时钟即可。原始Xv6的timervec没有使用，全部交给Rustsbi处理。如果发生时钟中断，就会调用thread_yield让出CPU，CPU会执行下一个TCB_RUNNABLE的线程。

下面是三个在调试过程中遇到的page fault异常：

4. **Instruction page fault** ：**scause值为12**

![pagefault1](../image/trap.assets/pagefault1.png)

5. **load page fault ：scause值为13**

![pagefault2](../image/trap.assets/pagefault2.png)

6. **store/amo page fault ：scause值为15**

![pagefault3](../image/trap.assets/pagefault3.png)

**TODO ：决赛我们将会在板卡上运行OS，并添加信号处理，usertrap会进一步完善和优化。**









### 系统调用的入口函数

如果sscause指明发生异常的原因是SYSCALL，那么就需要执行内核中的syscall函数处理系统调用。

首先需要确保内核线程没有被杀死，并且将trapframe中的epc+4，保证在执行完syscall后回到用户态能够顺序执行下一条指令。

由于在执行syscall时会发生中断，那么在执行syscall前需要及时开启中断。

进入sycall后，我们需要通过a7寄存器读取系统调用号，并且通过全局的系统调用表找到对应的入口函数，执行系统调用。需要注意的是a0寄存器保存的是返回值，所以需要更新trapframe的a0寄存器，及时接收syscall的返回值。

在syscall中加入STRACE可以方便调试，找到用户程序调用系统调用的整个过程。

系统调用表根据shell脚本生成，添加一个新的系统调用只需要修改syscall.tbl和编写系统调用的入口函数即可。





### thread_usertrap处理结束后会进入thread_usertrapret

```c
void thread_usertrapret();
```

一定要关闭中断，防止无法正确回到用户态。

设置stvec，使下次用户态发生trap自动跳转到userec。

将satp、内核态的栈顶指针复原、设置user trap 的处理函数地址在kernel_trap中。还需要将tp寄存器（保存的是CPU的id）保存在kernel_hartid中。

在返回U态前需要设置sstatus寄存器的SPP位和SPIE，确保结束后返回的是U态并且开始U态的中断。

由于在进入中断时保存了epc寄存器（发生异常时的地址，如果是syscall 还需要加4），所以还需要将trapframe中epc写入sepc中，为后续的userret 执行sret时，修改pc寄存器做准备。

由于返回到U态后，页表切换为了用户态的页表，所以需要将进程的页表写入satp寄存器。

从内核态返回到用户态的准备工作都结束了，接下来就需要执行userret返回用户态了。









### userret从内核态返回到用户态

在调用userret的时候第一个参数是用户态的页表，需要将器加载satp寄存器中。

```assembly
csrw satp, a0
```

然后是通过TRAPFRAME恢复在userec时保留的在trapframe中的寄存器（除了pc）。

由于此时用了a0寄存器作为中间寄存器，所以a0寄存器需要在最后恢复。

最后调用sret，从S态返回到用户态，并且设置pc寄存器为sepc的值。









### 内核态的trap处理

在进入thread_usertrap后会将kernelvec写入stvec寄存器。那么在内核态中发生的异常就会跳转到kernelvec中处理。

一旦在内核中发生了异常，就会进入kernelvec。首先就是将ra、sp等寄存器的内容放入栈中。然后调用kerneltrap。

kerneltrap主要就是分析sepc、sstatus和scause这三个寄存器分析在内核中发生的异常。在进行异常处理时需要确保sstatus的SPP为1，即异常发生在内核态。然后确保中断时关闭的（否则无法保证kerneltrap的正确执行）。

然后就算分析是否为设备中断。如果是非法为止的设备中断，就直接panic，并答应sepc、stval和scause便于调试。

如果在内核中发生了时钟中断，就需要调用thread_yield，完成线程的切换，调度器会选择就绪队列中的一个新的线程进行执行。

为了处理嵌套的trap，需要将sepc和status保存，在thread_yield结束后（即这个线程再次被调度），需要恢复这两个寄存器的内容：

```assembly
w_sepc(sepc);
w_sstatus(sstatus);
```

在执行完kerneltrap后，需要回到kernelvec，然后恢复进入kernelvec的保存在栈上的寄存器。然后执行sret后将epc写入pc，仍然为S态。







