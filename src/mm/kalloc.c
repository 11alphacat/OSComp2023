// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "common.h"
#include "param.h"
#include "memory/memlayout.h"
#include "atomic/spinlock.h"
#include "riscv.h"
#include "memory/list_alloc.h"
#include "memory/buddy.h"
#include "debug.h"
#include "kernel/cpu.h"

extern char end[];

static inline int get_pages_cpu(struct page *page) {
    return (page - pagemeta_start) / PAGES_PER_CPU;
}

static inline uint64 page_to_pa(struct page *page) {
    return (page - pagemeta_start) * PGSIZE + START_MEM;
}
static struct page *pa_to_page(uint64 pa) {
    ASSERT((pa - START_MEM) % PGSIZE == 0);
    return ((pa - START_MEM) / PGSIZE + pagemeta_start);
}

struct page *steal_mem(int cur_id, uint64 order) {
    struct page *page = NULL;
    for (int i = 0; i < NCPU; i++) {
        if (i == cur_id) {
            continue;
        }
        // TODO
        push_off();
        page = buddy_get_pages(&mempools[i], order);
        if (page != NULL) {
            return page;
        }
    }
    ASSERT(page == NULL);
    return page;
}

void *kalloc(void) {
    int order = 0;

    push_off();
    int id = cpuid();
    ASSERT(id >= 0 && id < NCPU);
    struct page *page = buddy_get_pages(&mempools[id], order);

    if (page == NULL) {
        page = steal_mem(id, order);
        if (page == NULL) {
            return 0;
        }
    }
    return (void *)page_to_pa(page);
}

void kfree(void *pa) {
    struct page *page = pa_to_page((uint64)pa);
    int id = get_pages_cpu(page);
    ASSERT(id >= 0 && id < NCPU);
    buddy_free_pages(&mempools[id], page);
}