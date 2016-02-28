#ifndef ATR_LANGUAGE_MODULE_H
#define ATR_LANGUAGE_MODULE_H

#include "anytrace/atr.h"

struct ATR_language_module {
    const char *lang_name;
    float lang_version;
    struct npr_symbol *hook_func_name;
};

ATR_EXPORT void ATR_language_init(struct ATR *atr);
ATR_EXPORT void ATR_language_fini(struct ATR *atr);

ATR_EXPORT void ATR_register_language(struct ATR *atr,
                                      const struct ATR_language_module *mod);

ATR_EXPORT struct npr_symbol *ATR_intern(const char *sym);

void ATR_load_language_module(struct ATR *atr);

#endif
