#include "common.h"
#include "atomic/spinlock.h"
#include "memory/mm_types.h"
#include "list.h"

#define MAX_ORDER 11
#define MAX_ORDER_NR_PAGES (1 << (MAX_ORDER - 1))

enum migratetype {
    MIGRATE_UNMOVABLE,   // 页是不可移动，通常kenel分配的内存
    MIGRATE_MOVABLE,     // 页是可移动或可迁移，皆在解决内存碎片，通常是用户空间申请的内存
    MIGRATE_RECLAIMABLE, // 页是可直接回收的，通常是文件缓存页
    MIGRATE_PCPTYPES,    /* the number of types on the pcp lists */
    // 表示per-cpu页框高速缓存的链表的迁移类型，针对的是order=0的页
    MIGRATE_HIGHATOMIC = MIGRATE_PCPTYPES,
    MIGRATE_CMA,
    MIGRATE_ISOLATE, /* can't allocate from here */
    MIGRATE_TYPES
};

struct free_area {
    struct list_head free_list[MIGRATE_TYPES];
    // 具有该大小的内存页块连接起来
    uint64 nr_free; // 内存页块的数目
                    // 对于0阶的表示以1页为单位计算，对于1阶的以2页为单位计算，n阶的以2的n次方为单位计算
};

struct zone {
    const char *name;
    struct spinlock lock;
    struct free_area free_area[MAX_ORDER];
} buddy_zone;

void buddy_init() {
    for (int i = 0; i < PAGE_NUMS; i++) {
        pages[i].order = 0;
        pages[i].isalloc = 1;
    }
    for (int i = 0; i < MAX_ORDER; i++) {
        INIT_LIST_HEAD(buddy_zone.free_area[i].free_list);
    }
}