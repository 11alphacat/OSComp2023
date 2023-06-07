# 内核中的类型
我们的内核中使用了若干类型别名，这里进行一个简单的介绍。

## 文件系统相关
- **type_t**  
``` c
typedef enum { FD_NONE,
               FD_PIPE,
               FD_INODE,
               FD_DEVICE } type_t;
```
用以表示一个 file 结构体所指的文件类型，或为管道文件，或为 Inode 文件，或为设备文件。


