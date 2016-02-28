#include "anytrace/atr-language-module.h"

static void
eval_hook(struct ATR *atr,
          struct ATR_backtracer *tr,
          void *hook_arg)
{
    puts("python");
    exit(1);
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

    hook_name_list[0] = ATR_intern("PyEval_FrameEx");
    mod.hook_func_name_list = hook_name_list;
    mod.hook_arg_list = hook_arg_list;
    mod.symbol_hook = eval_hook;

    ATR_register_language(atr, &mod);
}
