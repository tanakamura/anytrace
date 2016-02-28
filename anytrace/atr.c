#include <stdlib.h>
#include "anytrace/atr.h"
#include "anytrace/atr-impl.h"
#include "anytrace/atr-language-module.h"
#include "anytrace/atr-backtrace.h"

#include "npr/red-black-tree.h"
#include "npr/varray.h"

void
ATR_init(struct ATR *atr)
{
    npr_symbol_init();
    atr->last_error.code = ATR_NO_ERROR;
    atr->impl = malloc(sizeof(struct ATR_impl));

    atr->impl->cap_language = 1;
    atr->num_language = 0;
    atr->languages = malloc(sizeof(struct ATR_language_module) * 1);
    npr_symtab_init(&atr->impl->lang_module_hook_table, 16);

    ATR_load_language_module(atr);
}

void
ATR_free(struct ATR *st, void *p)
{
    free(p);
}

void
ATR_perror(struct ATR *atr)
{
    char *p = ATR_strerror(atr, &atr->last_error);
    fputs(p, stderr);
    fputc('\n', stderr);
    free(p);
}

void
ATR_fini(struct ATR *atr)
{
    ATR_error_clear(atr, &atr->last_error);
    free(atr->languages);
}

int
ATR_get_frame(struct ATR_stack_frame *frame,
              struct ATR *atr,
              struct ATR_process *proc,
              int tid)
{
    struct npr_rbtree visited;
    struct ATR_backtracer tr;

    struct npr_varray frames;

    int r = ATR_backtrace_init(atr, &tr, proc, tid);
    if (r < 0) {
        ATR_perror(atr);
        return;
    }

    npr_varray_init(&frames, 16, sizeof(struct ATR_stack_frame_entry));

    npr_rbtree_init(&visited);
    for (int depth=0; ; depth++) {
    }

    npr_rbtree_fini(&visited);

    return 0;
}
