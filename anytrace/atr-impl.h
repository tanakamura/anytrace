#ifndef ATR_IMPL_H
#define ATR_IMPL_H

#include "anytrace/atr.h"
#include "npr/int-map.h"

struct ATR_impl {
    int cap_language;           // internal
    struct npr_symtab lang_module_hook_table;
};

void ATR_load_language_module(struct ATR *atr);

#endif