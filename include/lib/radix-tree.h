#ifndef __RADIX_TREE_H__
#define __RADIX_TREE_H__
#include "common.h"
#include "atomic/ops.h"

#define RADIX_TREE_MAP_SHIFT 6
#define RADIX_TREE_MAP_SIZE (1UL << RADIX_TREE_MAP_SHIFT) // 1<<6 = 64
#define RADIX_TREE_MAX_TAGS 2
#define RADIX_TREE_TAG_LONGS ((RADIX_TREE_MAP_SIZE + BITS_PER_LONG - 1) / BITS_PER_LONG)
// (64+32-1)/32 = 95/32 = 2
#define RADIX_TREE_INDEX_BITS (8 /* CHAR_BIT */ * sizeof(uint64))
#define RADIX_TREE_MAX_PATH (DIV_ROUND_UP(RADIX_TREE_INDEX_BITS, RADIX_TREE_MAP_SHIFT))
// 64/6 上取整
// height   ||   max index
// 0             0
// 1             2^6-1
// 2             2^12-1
// 3             2^18-1
// ?             2^64-1
// ? = ⌈64 / 6⌉
#define RADIX_TREE_INDIRECT_PTR 1
// 1 : indirent node, 0 : data item
#define RADIX_TREE_MAP_MASK (RADIX_TREE_MAP_SIZE - 1) // 1<<6-1 = 64 -1

typedef unsigned int gfp_t;
#define __GFP_BITS_SHIFT 22 /* Room for 22 __GFP_FOO bits */
#define __GFP_BITS_MASK ((gfp_t)((1 << __GFP_BITS_SHIFT) - 1))

// root of radix tree
struct radix_tree_root {
    uint32 height;                 // height of radix tree
    struct radix_tree_node *rnode; // root node pointer
    gfp_t gfp_mask;
};

// node of radix tree
struct radix_tree_node {
    uint32 height;                                          // 叶子节点到树根的高度
    uint32 count;                                           // 当前节点的子节点个数，叶子节点的 count=0
    void *slots[RADIX_TREE_MAP_SIZE];                       // 每个slot对应一个子节点
    uint64 tags[RADIX_TREE_MAX_TAGS][RADIX_TREE_TAG_LONGS]; // 标签数组，用于存储各个元素的标记信息
};

// help for radix search
struct radix_tree_path {
    struct radix_tree_node *node; // 遍历路径上的当前节点
    int offset;                   // 当前节点在父节点中对应的槽位（slot）索引
};

// ops for tag
void *radix_tree_tag_set(struct radix_tree_root *root, uint64 index, uint32 tag);
void *radix_tree_tag_clear(struct radix_tree_root *root, uint64 index, uint32 tag);
int radix_tree_tag_get(struct radix_tree_root *root, uint64 index, uint32 tag);

// auxiliary functions
uint64 radix_tree_maxindex(uint height);

// allocate
struct radix_tree_node *radix_tree_node_alloc(struct radix_tree_root *root);
// search
void *radix_tree_lookup_node(struct radix_tree_root *root, uint64 index);
void **radix_tree_lookup_slot(struct radix_tree_root *root, uint64 index);
// insert
int radix_tree_insert(struct radix_tree_root *root, uint64 index, void *item);
// extend
int radix_tree_extend(struct radix_tree_root *root, unsigned long index);
// shrink
void radix_tree_shrink(struct radix_tree_root *root);
// delete
void *radix_tree_delete(struct radix_tree_root *root, uint64 index);
// free
void radix_tree_node_free(struct radix_tree_node *node);

#endif