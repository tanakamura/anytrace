#include <assert.h>
#include <stdlib.h>

#include "npr/red-black-tree.h"

int
main()
{
    struct npr_rbtree t;
    npr_rbtree_init(&t);

    npr_rbtree_insert(&t, 100, 1000);
    npr_rbtree_insert(&t, 200, 2000);
    npr_rbtree_insert(&t, 300, 3000);
    npr_rbtree_insert(&t, 400, 4000);
    npr_rbtree_insert(&t, 500, 5000);

    assert(npr_rbtree_has_key(&t, 100));
    assert(npr_rbtree_has_key(&t, 200));
    assert(npr_rbtree_has_key(&t, 300));
    assert(npr_rbtree_has_key(&t, 400));
    assert(npr_rbtree_has_key(&t, 500));

    //npr_rbtree_dump(&t);

    npr_rbtree_fini(&t);

    int size = 300;

    int *v = malloc(size * sizeof(int));

    npr_rbtree_init(&t);
    for (int i=0; i<300; i++) {
        v[i] = rand();
        npr_rbtree_insert(&t, v[i], 100);
    }

    for (int i=0; i<300; i++) {
        assert(npr_rbtree_has_key(&t, v[i]));
    }

    free(v);

    //npr_rbtree_dump(&t);
    npr_rbtree_fini(&t);
}