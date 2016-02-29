#include <stdlib.h>
#include <string.h>
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
    frame->frame_up_fail_reason.code = ATR_NO_ERROR;

    struct npr_rbtree visited;
    struct ATR_backtracer tr;

    struct npr_varray frames;

    int r = ATR_backtrace_init(atr, &tr, proc, tid);
    if (r < 0) {
        return -1;
    }

    npr_varray_init(&frames, 16, sizeof(struct ATR_stack_frame_entry));

    npr_rbtree_init(&visited);
    for (int depth=0; ; depth++) {
        int insert = npr_rbtree_insert(&visited, tr.cfa_regs[X8664_CFA_REG_RSP], 1);
        if (insert == 0) {
            /* loop is detected */
            ATR_set_error_code(atr, &frame->frame_up_fail_reason, ATR_FRAME_HAVE_LOOP);
            break;
        }

        struct ATR_addr_info ai;
        struct ATR_stack_frame_entry e;

        e.flags = ATR_FRAME_HAVE_PC;
        e.num_child_frame = 0;
        e.pc = tr.cfa_regs[X8664_CFA_REG_RIP];

        if (tr.state == ATR_BACKTRACER_OK) {
            ATR_file_lookup_addr_info(&ai, atr, &tr);

            if (ai.flags & ATR_ADDR_INFO_HAVE_SYMBOL) {
                e.flags |= ATR_FRAME_HAVE_SYMBOL;
                e.symbol = ai.sym;
                e.symbol_offset = ai.sym_offset;
            }

            e.flags |= ATR_FRAME_HAVE_OBJ_PATH;
            e.obj_path = strdup(tr.current_module.path->symstr);
        }

        VA_PUSH(struct ATR_stack_frame_entry, &frames, e);

        if (tr.state != ATR_BACKTRACER_OK) {
            ATR_error_move(atr, &frame->frame_up_fail_reason, &atr->last_error);
            break;
        }

        int r = ATR_backtrace_up(atr, &tr, proc);
        if (r != 0) {
            ATR_error_move(atr, &frame->frame_up_fail_reason, &atr->last_error);
            break;
        }
    }

    frame->num_entry = frames.nelem;
    frame->entries = npr_varray_malloc_close(&frames);

    npr_rbtree_fini(&visited);

    return 0;
}

void
ATR_frame_fini(struct ATR *atr,
               struct ATR_stack_frame *f)
{
    free(f->entries);
    ATR_error_clear(atr, &f->frame_up_fail_reason);
}

const char *
ATR_get_symstr(struct npr_symbol *sym)
{
    return sym->symstr;
}