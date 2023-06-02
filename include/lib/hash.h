#ifndef __HASH_H__
#define __HASH_H__

#include "atomic/spinlock.h"
#include "lib/list.h"
#include "param.h"

struct hash_node {
    union {
        int key_id;
        void *key_p;
        // char key_name[NAME_LONG_MAX];
        char key_name[NAME_LONG_MAX / 4];
    };
    void *value; // value
    struct list_head list;
};

enum hash_type { PID_MAP,
                 TID_MAP,
                 FUTEX_MAP,
                 INODE_MAP
};

struct hash_table {
    struct spinlock lock;
    enum hash_type type;
    uint64 size; // table size
    union {
        struct hash_node pid_head[NPROC];
        struct hash_node tid_head[NTCB];
        struct hash_node futex_head[FUTEX_NUM];
        struct hash_node inode_head[NINODE];
    };
};

struct hash_node *hash_entry(struct hash_table *table, void *key, enum hash_type type);
struct hash_node *hash_lookup(struct hash_table *table, void *key, enum hash_type type, struct hash_node **entry);
void hash_insert(struct hash_table *table, void *key, void *value, enum hash_type type);
void hash_delete(struct hash_table *table, void *key, enum hash_type type);
uint64 hash_str(char *name);
uint64 hash_val(struct hash_node *node, enum hash_type type);
uint64 hash_bool(struct hash_node *node, void *key, enum hash_type type);
void hash_assign(struct hash_node *node, void *key, enum hash_type type);
void hash_table_list_init(struct hash_table *map, enum hash_type type);

void hash_print(struct hash_table *map, enum hash_type type);

#endif
