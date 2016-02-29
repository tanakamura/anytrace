#ifndef ATR_IMPL_H
#define ATR_IMPL_H

#include "anytrace/atr.h"
#include "anytrace/atr-language-module.h"

#include "npr/int-map.h"

struct ATR_impl {
    int cap_language;           // internal
    struct npr_symtab lang_module_hook_table;
};

void ATR_load_language_module(struct ATR *atr);
int ATR_run_language_hook(struct ATR *atr,
                          struct ATR_backtracer *tr,
                          void *hook_arg,
                          struct npr_varray *machine_frame,
                          struct ATR_language_module *mod);

#endif