#include "lib/radix-tree.h"
#include "memory/allocator.h"
#include "atomic/ops.h"

// ===================ops for tag====================
static inline void tag_set(struct radix_tree_node *node, uint32 tag,
                           int offset) {
    set_bit(offset, node->tags[tag]);
}

static inline void tag_clear(struct radix_tree_node *node, uint32 tag,
                             int offset) {
    clear_bit(offset, node->tags[tag]);
}

static inline int tag_get(struct radix_tree_node *node, uint32 tag,
                          int offset) {
    return test_bit(offset, node->tags[tag]);
}

/*
 * Returns 1 if any slot in the node has this tag set.
 * Otherwise returns 0.
 */
static inline int any_tag_set(struct radix_tree_node *node, uint32 tag) {
    for (int idx = 0; idx < RADIX_TREE_TAG_LONGS; idx++) {
        if (node->tags[tag][idx])
            return 1;
    }
    return 0;
}

void *radix_tree_tag_set(struct radix_tree_root *root, uint64 index, uint32 tag) {
    return NULL;
}

void *radix_tree_tag_clear(struct radix_tree_root *root, uint64 index, uint32 tag) {
    return NULL;
}

int radix_tree_tag_get(struct radix_tree_root *root, uint64 index, uint32 tag) {
    return 0;
}

// ===================auxiliary functions====================
uint64 radix_tree_maxindex(uint height) {
    if (height > RADIX_TREE_MAX_PATH) {
        panic("radix_tree : error\n");
    }
    return (1 << (height * RADIX_TREE_MAP_SHIFT)) - 1; // at easy way to get maxindex
}

// Hint : in order to distinguish data item and radix_tree_node pointer
// * An indirect pointer (root->rnode pointing to a radix_tree_node, rather
// * than a data item) is signalled by the low bit set in the root->rnode pointer.
static inline int radix_tree_is_indirect_ptr(void *ptr) {
    return (int)((uint64)ptr & RADIX_TREE_INDIRECT_PTR);
}

static inline void *radix_tree_indirect_to_ptr(void *ptr) {
    return (void *)((uint64)ptr & ~RADIX_TREE_INDIRECT_PTR);
}

static inline void *radix_tree_ptr_to_indirect(void *ptr) {
    return (void *)((uint64)ptr | RADIX_TREE_INDIRECT_PTR);
}

// ===================init====================
struct radix_tree_node *radix_tree_node_init(struct radix_tree_root *root) {
    struct radix_tree_node *ret = NULL;
    ret = (struct radix_tree_node *)kzalloc(sizeof(struct radix_tree_node)); // kzalloc : with zero function
    if (ret == NULL) {
        panic("radix_tree_node_init : no memory\n");
    }
    return ret;
}

// ===================lookup====================

/*
 * is_slot == 1 : search for the slot.
 * is_slot == 0 : search for the node.
 */
static void *radix_tree_lookup_element(struct radix_tree_root *root,
                                       uint64 index, int is_slot) {
    uint32 height, shift;
    struct radix_tree_node *node, **slot;

    node = root->rnode;
    if (!radix_tree_is_indirect_ptr(node)) {
        // data item
        if (index > 0)
            return NULL;
        return is_slot ? (void *)&root->rnode : node;
    }

    node = radix_tree_indirect_to_ptr(node);

    height = node->height;
    if (index > radix_tree_maxindex(height))
        return NULL;

    shift = (height - 1) * RADIX_TREE_MAP_SHIFT;

    do {
        slot = (struct radix_tree_node **)(node->slots + ((index >> shift) & RADIX_TREE_MAP_MASK)); // offset mask
        shift -= RADIX_TREE_MAP_SHIFT;
        height--;
    } while (height > 0);

    return is_slot ? (void *)slot : node;
}

// Lookup the item at the position @index in the radix tree @root
void *radix_tree_lookup_node(struct radix_tree_root *root, uint64 index) {
    return radix_tree_lookup_element(root, index, 0);
}

// Lookup the slot corresponding to the position @index in the radix tree @root.
void **radix_tree_lookup_slot(struct radix_tree_root *root, uint64 index) {
    return (void **)radix_tree_lookup_element(root, index, 1);
}

// ===================insert====================
int radix_tree_insert(struct radix_tree_root *root, uint64 index, void *item) {
    return 0;
}

// ===================extend====================
int radix_tree_extend(struct radix_tree_root *root, unsigned long index) {
    return 0;
}

// ===================shrink====================
void radix_tree_shrink(struct radix_tree_root *root) {
    return;
}

// ===================delete====================
void *radix_tree_delete(struct radix_tree_root *root, uint64 index) {
    return 0;
}

// ===================free====================
// inline void radix_tree_node_free(struct radix_tree_node *node) {
//     return NULL;
// }