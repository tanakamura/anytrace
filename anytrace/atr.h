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

struct ATR {
    struct ATR_Error last_error; // public

    int cap_language;           // internal
    int num_language;           // public
    struct ATR_language_module *languages; // public
};

ATR_EXPORT void ATR_init(struct ATR *atr);
ATR_EXPORT void ATR_fini(struct ATR *atr);

ATR_EXPORT void ATR_free(struct ATR *atr, void *p);

ATR_EXPORT void ATR_perror(struct ATR *atr);

#ifdef __cplusplus
}
#endif

#endif