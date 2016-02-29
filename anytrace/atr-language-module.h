#ifndef ATR_LANGUAGE_MODULE_H
#define ATR_LANGUAGE_MODULE_H

#include "anytrace/atr.h"
#include "anytrace/atr-backtrace.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ATR_frame_builder;

typedef int (*ATR_language_module_hook_t)(struct ATR *atr,
                                          struct ATR_backtracer *tr,
                                          struct ATR_frame_builder *fb,
                                          void *hook_arg);

struct ATR_language_module {
#define ATR_LANGUAGE_USE_OWN_STACK (1<<0) // if this flag is on, frame of this language is chained to child.

    int flags;

    const char *lang_name;
    float lang_version;

    int num_symbol;
    struct npr_symbol **hook_func_name_list;
    void **hook_arg_list;

    ATR_language_module_hook_t symbol_hook;
};

ATR_EXPORT void ATR_language_init(struct ATR *atr);
ATR_EXPORT void ATR_language_fini(struct ATR *atr);

ATR_EXPORT void ATR_register_language(struct ATR *atr,
                                      const struct ATR_language_module *mod);


/* put frame to top */
ATR_EXPORT struct ATR_stack_frame_entry *ATR_frame_entry_open(struct ATR_frame_builder *fb);
ATR_EXPORT void ATR_frame_entry_set_func_name(struct ATR_stack_frame_entry *e,
                                              struct npr_symbol *func_name);
ATR_EXPORT void ATR_frame_entry_set_location(struct ATR_stack_frame_entry *e,
                                             int lineno,
                                             const char *path);
ATR_EXPORT void ATR_frame_entry_close(struct ATR_frame_builder *fb);

#ifdef __cplusplus
}
#endif


#endif
