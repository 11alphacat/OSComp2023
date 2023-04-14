#ifndef __FAT32_STACK_H__
#define __FAT32_STACK_H__

#include "fs/fat/fat32_fs.h"
#include "common.h"
#define CAPACITY 30

typedef dirent_l_t elemtype;

struct Stack {
    int top;
    elemtype data[CAPACITY];
};
typedef struct Stack Stack_t;

// 初始化栈
void stack_init(Stack_t *);

// 判断栈是否为空
int stack_is_empty(Stack_t *);

// 判断栈是否已满
int stack_is_full(Stack_t *);

// 入栈操作
void stack_push(Stack_t *, elemtype);

// 出栈操作
elemtype stack_pop(Stack_t *);

// 获取栈顶元素
elemtype stack_peek(Stack_t *);

#endif