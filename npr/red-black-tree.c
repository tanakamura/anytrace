#include "npr/red-black-tree.h"

#define BLACK 0
#define RED 1

void
npr_rbtree_init(struct npr_rbtree *t)
{
    npr_chunk_allocator_init(&t->alloc, sizeof(struct npr_rbtree_node), 16);
    t->root = NULL;
}

void
npr_rbtree_fini(struct npr_rbtree *t)
{
    npr_chunk_allocator_fini(&t->alloc);
}

static struct npr_rbtree_node *
alloc_node(struct npr_rbtree *t)
{
    struct npr_rbtree_node *n = npr_chunk_allocator_alloc(&t->alloc);
    return n;
}

static struct npr_rbtree_node *
rotate_right(struct npr_rbtree_node *X0)
{
    /* <before>
     *     X0
     *    / \
     *   X1  X2
     *  / \
     * X3  X4
     *
     * <after>
     *     X1
     *    / \
     *   X3  X0
     *      /  \
     *     X4  X2
     */

    struct npr_rbtree_node *X1 = X0->left;
    X0->left = X1->right;
    X1->right = X0;
    X1->color = X0->color;
    X0->color = RED;
    return X1;
}

static struct npr_rbtree_node *
rotate_left(struct npr_rbtree_node *X1)
{
    /* <before>
     *     X1
     *    / \
     *   X3  X0
     *      /  \
     *     X4  X2
     *
     * <after>
     *     X0
     *    / \
     *   X1  X2
     *  / \
     * X3  X4
     */

    struct npr_rbtree_node *X0 = X1->right;
    X1->right = X0->left;
    X0->left = X1;
    X0->color = X1->color;
    X1->color = RED;
    return X0;
}

static void
flip_color(struct npr_rbtree_node *n)
{
    n->color = !n->color;
    n->left->color = !n->left->color;
    n->right->color = !n->right->color;
}

static int
is_red(struct npr_rbtree_node *n)
{
    if (n == NULL) {
        return 0;
    }

    return n->color == RED;
}

static struct npr_rbtree_node *
insert(struct npr_rbtree *t, struct npr_rbtree_node *n,
       npr_rbtree_key_t key,
       npr_rbtree_value_t v,
       int *insert_new)
{
    if (n == NULL) {
        n = alloc_node(t);
        n->left = NULL;
        n->right = NULL;
        n->v = v;
        n->key = key;
        n->color = RED;
        *insert_new = 1;

        return n;
    }

    if (is_red(n->left) && is_red(n->right)) {
        flip_color(n);
    }

    if (key == n->key) {
        n->v = v;
    } else if (key < n->key) {
        n->left = insert(t, n->left, key, v, insert_new);
    } else {
        n->right = insert(t, n->right, key, v, insert_new);
    }

    if (is_red(n->right)) {
        n = rotate_left(n);
    }

    if (is_red(n->left) && is_red(n->left->left)) {
        n = rotate_right(n);
    }

    return n;
}


int
npr_rbtree_insert(struct npr_rbtree *t, npr_rbtree_key_t key, npr_rbtree_value_t v)
{
    int insert_new = 0;
    struct npr_rbtree_node *root = insert(t, t->root, key, v, &insert_new);

    t->root = root;
    root->color = BLACK;

    return insert_new;
}


/* ... 
void
npr_rbtree_delete(struct npr_rbtree *t, npr_rbtree_key_t key)
{
}
*/



static void
traverse(struct npr_rbtree_node *n, npr_rbtree_traverse_func_t f, void *arg)
{
    if (n == NULL) {
        return;
    }

    traverse(n->left, f, arg);
    f(n, arg);
    traverse(n->right, f, arg);
}


void
npr_rbtree_traverse(struct npr_rbtree *t, npr_rbtree_traverse_func_t f, void *arg)
{
    traverse(t->root, f, arg);
}



static int
has_key(struct npr_rbtree_node *n, npr_rbtree_key_t key)
{
    if (n == NULL) {
        return 0;
    }

    if (key == n->key) {
        return 1;
    }

    if (key < n->key) {
        return has_key(n->left, key);
    } else {
        return has_key(n->right, key);
    }
}

int
npr_rbtree_has_key(struct npr_rbtree *t, npr_rbtree_key_t key)
{
    return has_key(t->root, key);
}


static void
dump_node(struct npr_rbtree_node *n, int depth)
{
    if (n) {
        dump_node(n->left, depth+1);
        for (int di=0; di<depth; di++) {
            printf("    ");
        }
        if (n->color == BLACK) {
            printf("{B, %d,%d}\n", (int)n->key, (int)n->v);
        } else {
            printf("{R, %d,%d}\n", (int)n->key, (int)n->v);
        }

        dump_node(n->right, depth+1);
    }
}



void
npr_rbtree_dump(struct npr_rbtree *t)
{
    dump_node(t->root, 0);
}