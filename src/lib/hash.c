
#include "memory/allocator.h"
#include "atomic/spinlock.h"
#include "lib/hash.h"
#include "debug.h"

struct hash_table pid_map = {.lock = INIT_SPINLOCK(pid_hash_table),
                             .type = PID_MAP,
                             .size = NPROC};
struct hash_table tid_map = {.lock = INIT_SPINLOCK(tid_hash_table),
                             .type = TID_MAP,
                             .size = NTCB};
struct hash_table futex_map = {.lock = INIT_SPINLOCK(futex_hash_table),
                               .type = TID_MAP,
                               .size = FUTEX_NUM};
// find the table entry given the table，type and key
struct hash_node *hash_entry(struct hash_table *table, void *key, enum type_table type) {
    uint64 hash_val = 0;
    struct hash_node *node = NULL;

    switch (type) {
    case PID_MAP:
        hash_val = *(int *)key % table->size;
        node = table->pid_head + hash_val;
        break;
    case TID_MAP:
        hash_val = *(int *)key % table->size;
        node = table->tid_head + hash_val;
        break;
    case FUTEX_MAP:
        hash_val = (uint64)key % table->size;
        node = table->futex_head + hash_val;
        break;
    case NAME_MAP:
        hash_val = hash_str((char *)key) % table->size;
        node = table->inode_head + hash_val;
        break;
    default:
        panic("this type is invalid\n");
    }
    acquire(&table->lock);
    return node;
}

// lookup the hash table
struct hash_node *hash_lookup(struct hash_table *table, void *key, enum type_table type, struct hash_node **entry) {
    struct hash_node *_entry = hash_entry(table, key, type);

    if (entry)
        *entry = _entry;
    struct hash_node *node_cur = NULL;
    struct hash_node *node_tmp = NULL;
    list_for_each_entry_safe(node_cur, node_tmp, &_entry->list, list) {
        if (hash_bool(node_cur, key, type)) {
            release(&table->lock);
            return node_cur;
        }
    }
    release(&table->lock);
    return NULL;
}

// insert the hash node into the table
void hash_insert(struct hash_table *table, void *key, void *value, enum type_table type) {
    struct hash_node *entry = NULL;
    struct hash_node *node = hash_lookup(table, key, type, &entry);

    acquire(&table->lock);
    struct hash_node *node_new;
    if (node == NULL) {
        node_new = (struct hash_node *)kmalloc(sizeof(struct hash_node));
        hash_assign(node_new, key, type);
        node_new->value = value;
        INIT_LIST_HEAD(&node_new->list);
        list_add_tail(&node_new->list, &(entry->list));
    } else {
        node->value = value;
    }
    release(&table->lock);
}

// delete the inode given the key
void hash_delete(struct hash_table *table, void *key, enum type_table type) {
    struct hash_node *node = hash_lookup(table, key, type, NULL);

    acquire(&table->lock);
    if (node != NULL) {
        list_del_reinit(&node->list);
        kfree(node);
    } else {
        printfRed("hash delete : this key doesn't existed\n");
    }
    release(&table->lock);
}
// 803d1c60
// hash value given a string
uint64 hash_str(char *name) {
    uint64 hash_val = 0;
    size_t len = strlen(name);
    for (size_t i = 0; i < len; i++) {
        hash_val = hash_val * 31 + (uint64)name[i];
    }
    return hash_val;
}

// choose the key of hash node given type of hash table
uint64 hash_val(struct hash_node *node, enum type_table type) {
    uint64 hash_val = 0;
    switch (type) {
    case PID_MAP:
    case TID_MAP:
        hash_val = node->key_id;
        break;
    case FUTEX_MAP:
        hash_val = (uint64)node->key_p;
    case NAME_MAP:
        hash_val = hash_str(node->key_name);
        break;

    default:
        panic("this type is invalid\n");
    }
    return hash_val;
}

// judge the key of hash node given key and type of hash table
uint64 hash_bool(struct hash_node *node, void *key, enum type_table type) {
    uint64 ret = 0;
    switch (type) {
    case PID_MAP:
    case TID_MAP:
        ret = (node->key_id == *(int *)key);
        break;
    case FUTEX_MAP:
        ret = ((uint64)node->key_p == (uint64)key);
        break;
    case NAME_MAP:
        ret = (hash_str(node->key_name) == hash_str((char *)key));
        break;
    default:
        panic("this type is invalid\n");
    }
    return ret;
}

// assign the value of hash node given key and table type
void hash_assign(struct hash_node *node, void *key, enum type_table type) {
    switch (type) {
    case PID_MAP:
    case TID_MAP:
        node->key_id = *(int *)key;
        break;
    case FUTEX_MAP:
        node->key_p = key;
        break;
    case NAME_MAP:
        safestrcpy(node->key_name, (char *)key, strlen((char *)key));
        break;
    default:
        panic("this type is invalid\n");
    }
}

void hash_table_init(struct hash_table *map, enum type_table type) {
    for (int i = 0; i < map->size; i++) {
        switch (type) {
        case PID_MAP:
            INIT_LIST_HEAD(&map->pid_head[i].list);
            break;
        case TID_MAP:
            INIT_LIST_HEAD(&map->tid_head[i].list);
            break;
        case FUTEX_MAP:
            INIT_LIST_HEAD(&map->futex_head[i].list);
            break;
        case NAME_MAP:
            INIT_LIST_HEAD(&map->inode_head[i].list);
            break;
        default:
            panic("this type is invalid\n");
        }
    }
}

void hash_tables_init() {
    hash_table_init(&pid_map, PID_MAP);
    hash_table_init(&tid_map, TID_MAP);
    hash_table_init(&futex_map, FUTEX_MAP);
}

void hash_print(struct hash_table *map, enum type_table type) {
    acquire(&map->lock);
    struct hash_node *node_cur = NULL;
    struct hash_node *node_tmp = NULL;
    for (int i = 0; i < map->size; i++) {
        switch (type) {
        case PID_MAP:
            list_for_each_entry_safe(node_cur, node_tmp, &map->pid_head[i].list, list) {
                printf("PID entry: %d, next : %x prev: %x\n", i, node_cur->list.next, node_cur->list.prev);
            }
            break;
        case TID_MAP:
            list_for_each_entry_safe(node_cur, node_tmp, &map->tid_head[i].list, list) {
                printf("TID entry: %d, next : %x prev: %x\n", i, node_cur->list.next, node_cur->list.prev);
            }
            break;
        case FUTEX_MAP:
            INIT_LIST_HEAD(&map->futex_head[i].list);
            break;
        case NAME_MAP:
            INIT_LIST_HEAD(&map->inode_head[i].list);
            break;
        default:
            panic("this type is invalid\n");
        }
    }

    release(&map->lock);
}