#include "anytrace/atr-language-module.h"
#include <stdio.h>
#include <stdlib.h>

static int
eval_hook(struct ATR *atr,
          struct ATR_backtracer *tr,
          struct ATR_frame_builder *fb,
          void *hook_arg)
{
    /* PyObject *PyEval_EvalFrameEx(PyFrameObject *f, int throwflag) */

    /* on x86-64, rdi holds "f" */
    printf("%p\n", (void*)tr->cfa_regs[5]);

    return 0;
}

void
ATR_language_init(struct ATR *atr)
{
    struct ATR_language_module mod;

    struct npr_symbol *hook_name_list[1];
    void *hook_arg_list[1] = {NULL};

    mod.lang_name = "Python";
    mod.lang_version = 2.7f;
    mod.num_symbol = 1;

    hook_name_list[0] = ATR_intern("PyEval_EvalFrameEx");
    mod.hook_func_name_list = hook_name_list;
    mod.hook_arg_list = hook_arg_list;
    mod.symbol_hook = eval_hook;

    ATR_register_language(atr, &mod);
}
