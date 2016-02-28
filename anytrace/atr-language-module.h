#ifndef ATR_LANGUAGE_MODULE_H
#define ATR_LANGUAGE_MODULE_H

#include "anytrace/atr.h"
#include "anytrace/atr-backtrace.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ATR_language_module {
    const char *lang_name;
    float lang_version;

    int num_symbol;
    struct npr_symbol **hook_func_name_list;
    void **hook_arg_list;

    void (*symbol_hook)(struct ATR *atr,
                        struct ATR_backtracer *tr,
                        void *hook_arg);
};

ATR_EXPORT void ATR_language_init(struct ATR *atr);
ATR_EXPORT void ATR_language_fini(struct ATR *atr);

ATR_EXPORT void ATR_register_language(struct ATR *atr,
                                      const struct ATR_language_module *mod);

ATR_EXPORT struct npr_symbol *ATR_intern(const char *sym);


#ifdef __cplusplus
}
#endif


#endif
