#include "memory/buddy.h"
#include "common.h"
#include "memory/memlayout.h"
#include "list.h"
#include "riscv.h"
#include "debug.h"

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.
uint64 npages;

static struct page *merge_page(struct phys_mem_pool *pool, struct page *page);
static struct page *split_page(struct phys_mem_pool *pool, uint64 order, struct page *page);
void init_buddy(struct page *start_page, uint64 start_addr, uint64 page_num);
struct phys_mem_pool mempools;

static inline uint64 page_to_offset(struct phys_mem_pool *pool, struct page *page) {
    return (page - pool->page_metadata) * PGSIZE;
}

static struct page *offset_to_page(struct phys_mem_pool *pool, uint64 offset) {
    ASSERT(offset % PGSIZE == 0);
    return (offset / PGSIZE + pool->page_metadata);
}

static struct page *get_buddy(struct phys_mem_pool *pool, struct page *page) {
    ASSERT(page->order < BUDDY_MAX_ORDER);
    uint64 this_off = page_to_offset(pool, page);
    uint64 buddy_off = this_off ^ (1UL << (page->order + 12));

    /* Check whether the buddy belongs to pool */
    if (buddy_off >= pool->mem_size) {
        return NULL;
    }

    return offset_to_page(pool, (uint64)buddy_off);
}

void mm_init() {
    init_buddy((struct page *)PGROUNDUP((uint64)end), (uint64)START_MEM, NPAGES);
}

void init_buddy(struct page *start_page, uint64 start_addr, uint64 page_num) {
    // Log("pagemeta start: %x", start_page);
    // Log("page_num %d", page_num);
    // Log("pagemeta end: %x", (uint64)start_page + page_num * sizeof(struct page));
    ASSERT((uint64)start_page + page_num * sizeof(struct page) < start_addr);
    mempools.start_addr = start_addr;
    mempools.page_metadata = start_page;
    mempools.mem_size = PGSIZE * page_num;

    // Log("start_page_metadata: %#x", mempools.page_metadata);
    // Log("start_addr: %#x", mempools.start_addr);
    // Log("mem size: %#x", mempools.mem_size);

    /* Init the spinlock */
    initlock(&mempools.lock, "buddy_phy_mem_pools_lock");

    /* Init the free lists */
    for (int order = 0; order <= BUDDY_MAX_ORDER; ++order) {
        INIT_LIST_HEAD(&mempools.freelists[order].lists);
        mempools.freelists[order].num = 0;
    }

    /* Clear the page_metadata area. */
    memset(mempools.page_metadata, 0, page_num * sizeof(struct page));

    /* Init the page_metadata area. */
    struct page *page;
    for (int page_idx = 0; page_idx < page_num; ++page_idx) {
        page = start_page + page_idx;
        page->allocated = 1;
        page->order = 0;
    }

    /* Put each physical memory page into the free lists. */
    for (int page_idx = 0; page_idx < page_num; ++page_idx) {
        page = start_page + page_idx;
        buddy_free_pages(&mempools, page);
    }
    // Log("finish initialization");

    /* make sure the buddy_free_pages works correctly */
    uint64 memsize = 0;
    for (int i = 0; i <= BUDDY_MAX_ORDER; i++) {
        Log("%d order chunks num: %d", i, mempools.freelists[i].num);
        memsize += mempools.freelists[i].num * PGSIZE * (1 << i);
    }
    Log("memsize: %u", memsize / 1024 / 1024);
    ASSERT(memsize == mempools.mem_size);
    return;
}

struct page *buddy_get_pages(struct phys_mem_pool *pool, uint64 order) {
    ASSERT(order <= BUDDY_MAX_ORDER);

    struct page *page = NULL;
    struct list_head *lists;

    acquire(&pool->lock);
    for (int i = order; i <= BUDDY_MAX_ORDER; i++) {
        lists = &pool->freelists[i].lists;
        if (!list_empty(lists)) {
            page = list_first_entry(lists, struct page, list);
            list_del(&page->list);
            pool->freelists[page->order].num--;
            break;
        }
    }

    if (page == NULL) {
        Log("there is no 2^%d mem!", order);
        release(&pool->lock);
        return NULL;
    }

    if (page->order > order) {
        page = split_page(pool, order, page);
    }
    page->allocated = 1;
    ASSERT(page->order == order);
    release(&pool->lock);
    return page;
}

static struct page *split_page(struct phys_mem_pool *pool, uint64 order, struct page *page) {
    ASSERT(page->order > order);

    while (page->order > order) {
        page->order--;
        struct page *buddy = get_buddy(pool, page);
        ASSERT(buddy->allocated == 0);
        buddy->order = page->order;

        list_add(&buddy->list, &pool->freelists[buddy->order].lists);
        pool->freelists[buddy->order].num++;
    }
    return page;
}

void buddy_free_pages(struct phys_mem_pool *pool, struct page *page) {
    acquire(&pool->lock);
    page->allocated = 0;

    if (page->order < BUDDY_MAX_ORDER) {
        page = merge_page(pool, page);
    }

    struct list_head *list = &pool->freelists[page->order].lists;
    list_add(&page->list, list);
    pool->freelists[page->order].num++;
    release(&pool->lock);
}

static struct page *merge_page(struct phys_mem_pool *pool, struct page *page) {
    if (page->order == BUDDY_MAX_ORDER) {
        return page;
    }

    static int max = 0;

    struct page *buddy = get_buddy(pool, page);
    if (buddy == NULL) {
        // this page doesn't have buddy
        return page;
    }

    ASSERT(buddy != NULL);
    if (buddy->allocated == 0 && buddy->order == page->order) {
        // get the buddy page out of the list
        list_del(&buddy->list);
        pool->freelists[buddy->order].num--;

        // merge buddy with current page
        struct page *merge = ((uint64)buddy < (uint64)page ? buddy : page);
        merge->order++;
        if (merge->order > max) {
            max = merge->order;
            Log("%d", max);
        }
        return merge_page(pool, merge);
    } else {
        return page;
    }
}