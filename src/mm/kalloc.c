// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "common.h"
#include "param.h"
#include "memory/memlayout.h"
#include "atomic/spinlock.h"
#include "lib/riscv.h"
#include "memory/allocator.h"
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

uint64 size_to_page_order(uint64 size) {
    uint64 order;
    uint64 page_num;
    uint64 tmp;

    order = 0;
    page_num = ROUND_UP(size, PGSIZE) / PGSIZE;
    tmp = page_num;

    while (tmp > 1) {
        tmp >>= 1;
        order += 1;
    }

    if (page_num > (1 << order)) {
        order += 1;
    }

    return order;
}

void *kmalloc(size_t size) {
    uint64 order;
    if (size <= PGSIZE) {
        order = 0;
    } else {
        order = size_to_page_order(size);
    }

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

    // acquire(&page->lock);
    // ASSERT(page->count == 0);
    ASSERT(atomic_read(&page->refcnt)==0);
    atomic_set(&page->refcnt, 1);
    // page->count = 1;
    // release(&page->lock);
    return (void *)page_to_pa(page);
}

void *kzalloc(size_t size) {
    void *ptr;

    ptr = kmalloc(size);

    /* lack of memory */
    if (ptr == NULL)
        return NULL;

    memset(ptr, 0, size);
    return ptr;
}

/* compatible with the old kalloc call, use kmalloc instead */
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

    // acquire(&page->lock);
    ASSERT(atomic_read(&page->refcnt)==0);
    atomic_set(&page->refcnt, 1);
    // ASSERT(page->count == 0);
    // page->count = 1;
    // release(&page->lock);
    return (void *)page_to_pa(page);
}

void kfree(void *pa) {
    struct page *page = pa_to_page((uint64)pa);
    acquire(&page->lock);
    // ASSERT(page->count >= 1);
    ASSERT(atomic_read(&page->refcnt)>=1);
    // page->count--;
    atomic_dec_return(&page->refcnt);
    if (atomic_read(&page->refcnt) >= 1) {
        release(&page->lock);
        return;
    }
    // ASSERT(page->count == 0);
    ASSERT(atomic_read(&page->refcnt)==0);
    release(&page->lock);

    ASSERT(page->allocated == 1);
    int id = get_pages_cpu(page);
    ASSERT(id >= 0 && id < NCPU);
    buddy_free_pages(&mempools[id], page);
}

void share_page(uint64 pa) {
    struct page *page = pa_to_page(pa);
    // acquire(&page->lock);
    // ASSERT(page->count >= 1);
    ASSERT(atomic_read(&page->refcnt)>=1);
    atomic_inc_return(&page->refcnt);
    // page->count++;

    // release(&page->lock);
    return;
}
