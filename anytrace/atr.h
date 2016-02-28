#ifndef ATR_H
#define ATR_H

#include "anytrace/atr-errors.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#ifdef ATR_BUILD_LIB
#define ATR_EXPORT __declspec(dllexport)
#else
#define ATR_EXPORT __declspec(dllimport)
#endif

#else

#define ATR_EXPORT __attribute__((visibility("default")))

#endif

struct ATR_impl;

struct ATR {
    struct ATR_Error last_error;

    int num_language;
    struct ATR_language_module *languages;

    struct ATR_impl *impl;
};

ATR_EXPORT void ATR_init(struct ATR *atr);
ATR_EXPORT void ATR_fini(struct ATR *atr);

ATR_EXPORT void ATR_free(struct ATR *atr, void *p);


/*
 * toplevel
 *     [0] C frame, num_child_frame = 0
 *     [1] C frame, num_child_frame = 2
 *           child_frame
 *           [0] Python frame num_child_frame = 0
 *           [1] Python frame num_child_frame = 0
 *     [2] C frame, num_child_frame = 0
 *     [3] C frame, num_child_frame = 0
 *     [4] Java frame, num_child_frame = 0
 *     [5] C, num_child_frame = 0
 *
 */

enum ATR_frame_lang_code {
    ATR_FRAME_LANG_C,
    ATR_FRAME_LANG_EXT
};

struct ATR_stack_frame {
    enum ATR_frame_lang_code lang;
    int ext_lang_id;            // index of ATR::languages, valid if lang == ATR_FRAME_LANG_EXT

#define ATR_FRAME_HAVE_SYMBOL (1<<0)
#define ATR_FRAME_HAVE_LOCATION (1<<1)
#define ATR_FRAME_HAVE_BOTTOM_LANG (1<<2)
    int flags;

    struct npr_symbol *func_name; // valid if HAVE_SYMBOL

    int lineno;                 // valid if HAVE_LOCATION
    char *path;                 // valid if HAVE_LOCATION

    int num_child_frame;
    struct ATR_stack_frame *child_frame;
};


ATR_EXPORT void ATR_perror(struct ATR *atr);

#ifdef __cplusplus
}
#endif

#endif