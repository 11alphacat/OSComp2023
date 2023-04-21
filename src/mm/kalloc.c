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

static inline uint64 page_to_pa(struct phys_mem_pool *pool, struct page *page) {
    return (page - pool->page_metadata) * PGSIZE + pool->start_addr;
}

static struct page *pa_to_page(struct phys_mem_pool *pool, uint64 pa) {
    ASSERT((pa - pool->start_addr) % PGSIZE == 0);
    return ((pa - pool->start_addr) / PGSIZE + pool->page_metadata);
}

void *kalloc(void) {
    struct page *page = buddy_get_pages(&mempools, 0);
    if (page == NULL) {
        return 0;
    } else {
        return (void *)page_to_pa(&mempools, page);
    }
}

void kfree(void *pa) {
    struct page *page = pa_to_page(&mempools, (uint64)pa);
    buddy_free_pages(&mempools, page);
}