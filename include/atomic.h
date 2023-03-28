#ifndef _H_ATOMIC_
#define _H_ATOMIC_

// 原子操作数据结构
typedef struct {
	volatile int counter;
} atomic_t;

// 初始化
#define ATOMIC_INITIALIZER { 0 }
#define ATOMIC_INIT(i)	  { (i) }

// 加、自增、自减、获取、counter
#define atomic_add(ptr, i) __atomic_add(ptr, i)
#define atomic_inc(ptr) __atomic_add(ptr, 1)
#define atomic_dec(ptr) __atomic_add(ptr, -1)
#define atomic_get(ptr) ((ptr)->counter)

// 原子性的加
static inline int __atomic_add(atomic_t *v, int i) {
    register int ret;						
	__asm__ __volatile__ (					
		"	amoadd.w.aqrl  %1, %2, %0"	
		: "+A" (v->counter), "=r" (ret)	
		: "r" (i)
		: "memory");
	return ret;					
}
#endif