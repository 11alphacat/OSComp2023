#ifndef __MM_TYPES_H__
#define __MM_TYPES_H__

#include "common.h"
#include "memory/memlayout.h"
#include "riscv.h"
#include "atomic/atomic.h"
#include "list.h"

#define PAGE_NUMS ((PHYSTOP - KERNBASE) / PGSIZE)

enum pageflags {
    PG_locked,
    PG_error,
    PG_referenced,
    PG_uptodate,
    PG_dirty,
    PG_lru,
    PG_active,
    PG_slab,
    PG_writeback
};
struct Page_INFO {
    atomic_t _count;
    uint64 flags;
    uint8 isalloc;
    uint8 order;
    struct list_head lru;
};

extern struct Page_INFO pages[PAGE_NUMS];

#endif // __MM_TYPES_H__
