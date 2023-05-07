#include "fs/fat/fat32_stack.h"
#include "common.h"

// 初始化栈
void stack_init(Stack_t *stack) {
    stack->top = -1;
}

// 判断栈是否为空
int stack_is_empty(Stack_t *stack) {
    return stack->top == -1;
}

// 判断栈是否已满
int stack_is_full(Stack_t *stack) {
    return stack->top == CAPACITY - 1;
}

// 入栈操作
void stack_push(Stack_t *stack, elemtype item) {
    if (stack_is_full(stack)) {
        panic("Stack Overflow!");
    }
    stack->data[++stack->top] = item;
}

// 出栈操作
elemtype stack_pop(Stack_t *stack) {
    if (stack_is_empty(stack)) {
        panic("Stack Underflow!");
    }
    return stack->data[stack->top--];
}

// 获取栈顶元素
elemtype stack_peek(Stack_t *stack) {
    if (stack_is_empty(stack)) {
        panic("Stack is empty!");
    }
    return stack->data[stack->top];
}