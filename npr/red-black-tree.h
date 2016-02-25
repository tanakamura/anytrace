#ifndef NPR_REDBLACK_TREE
#define NPR_REDBLACK_TREE

#include <stdint.h>
#include "npr/chunk-alloc.h"

typedef uintptr_t npr_rbtree_key_t;
typedef uintptr_t npr_rbtree_value_t;

struct npr_rbtree_node {
    struct npr_rbtree_node *left, *right;
    npr_rbtree_value_t v;
    npr_rbtree_key_t key;
    int color;
};

struct npr_rbtree {
    struct npr_chunk_allocator alloc;
    struct npr_rbtree_node *root;
};

void npr_rbtree_init(struct npr_rbtree *t);
void npr_rbtree_fini(struct npr_rbtree *t);

/* return 1 if inserted, or
 * return 0 if tree already has key. (value will be updated) */
int npr_rbtree_insert(struct npr_rbtree *t,
                      npr_rbtree_key_t key,
                      npr_rbtree_value_t v);

void npr_rbtree_delete(struct npr_rbtree *t, npr_rbtree_key_t key);

int npr_rbtree_has_key(struct npr_rbtree *t, npr_rbtree_key_t key);

typedef void (*npr_rbtree_traverse_func_t)(struct npr_rbtree_node *n, void *arg);

void npr_rbtree_traverse(struct npr_rbtree *t, npr_rbtree_traverse_func_t f, void *arg); 

void npr_rbtree_dump(struct npr_rbtree *t); /* for debug */

#endif