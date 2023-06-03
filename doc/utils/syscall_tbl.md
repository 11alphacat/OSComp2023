原先的xv6如果想要添加一个新的系统调用，过程十分复杂，需要4个步骤：

比如我们需要添加一个clone这个系统调用。

首先需要在下面的代码中添加一个系统调用表的entry：

```c
static uint64 (*syscalls[])(void) = {
...
[SYS_clone]    sys_clone
};
```

然后添加一个sys_clone的声明：

```c
...
extern uint64 sys_clone(void);
```

然后是在syscall.c中添加一个系统调用号的宏定义：

```c
...
#define SYS_clone 220
```

最后是给sys_clone写一个定义：

```c
uint64
sys_clone(void) {
	...
}
```

但其实并不需要这么复杂，我们只需要利用#include的一点点特性，即“无情”的拷贝就可以只需要用一个30行的shell脚本优雅地完成这优化这个过程（借鉴了Linux的做法）。

只需要下面的两个步骤就可以添加一个clone系统调用：

1. 在syscall.tbl中添加下面的一项

```c
220   clone            sys_clone
```

2. 然后写sys_clone的定义即可

```
uint64
sys_clone(void) {
	...
}
```

即只需要在tbl中按照如下的格式添加一个表项，然后就可以写对应的系统调用函数定义了。

```c
#系统调用号   宏定义(会补上SYS_的前缀)    函数指针名  
```

本质就是通过shell脚本生成了系统调用表项，系统调用号，系统调用函数的声明。







