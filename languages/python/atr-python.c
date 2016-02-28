#include "anytrace/atr-language-module.h"

void
ATR_language_init(struct ATR *atr)
{
    struct ATR_language_module mod;

    mod.lang_name = "Python";
    mod.lang_version = 2.7f;
    mod.hook_func_name = ATR_intern("PyEval_FrameEx");

    ATR_register_language(atr, &mod);

}
