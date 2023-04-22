#ifndef __BUDDY_H__
#define __BUDDY_H__

#include "common.h"
#include "atomic/atomic.h"
#include "list.h"
#include "atomic/spinlock.h"
#include "param.h"
#include "riscv.h"
#include "memory/memlayout.h"

/*
    +---------------------------+ <-- rust-sbi jump to 0x80200000
    |     kernel img region     |
    +---------------------------+ <-- kernel end
    |    page_metadata region   |
    +---------------------------+ <-- START_MEM
    |          MEMORY           |
    |           ...             |
    +---------------------------+ <-- PHYSTOP
*/

/* configuration option */
#define START_MEM 0x80400000
#define BUDDY_MAX_ORDER 13

#define NPAGES (((PHYSTOP)-START_MEM) / (PGSIZE))
#define PAGES_PER_CPU (NPAGES / NCPU)
extern struct page *pagemeta_start;

struct page {
    // use for buddy system
    int allocated;
    int order;
    struct list_head list;

    // use for cow/lazy
    // atomic_t count;
};

struct free_list {
    struct list_head lists;
    int num;
};

struct phys_mem_pool {
    uint64 start_addr;
    uint64 mem_size;
    /*
	 * The start virtual address (for used in kernel) of
	 * the metadata area of this pool.
	 */
    struct page *page_metadata;

    struct spinlock lock;

    /* The free list of different free-memory-chunk orders. */
    struct free_list freelists[BUDDY_MAX_ORDER + 1];
};
extern struct phys_mem_pool mempools[NCPU];

void buddy_free_pages(struct phys_mem_pool *pool, struct page *page);
struct page *buddy_get_pages(struct phys_mem_pool *pool, uint64 order);

#endif // __BUDDY_H__