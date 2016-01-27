#ifndef ANYTRACE_ERROR_H
#define ANYTRACE_ERROR_H

#include <stdint.h>

#include "npr/int-map.h"
#include "npr/mempool.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ATR;

enum ATR_error_code {
    ATR_NO_ERROR,

    ATR_LIBC_ERROR,
    ATR_LIBC_PATH_ERROR,

    ATR_INVALID_ARGUMENT,
};

struct ATR_Error {
    enum ATR_error_code code;

    union {
        struct {
            int errno_;
        }libc;

        struct {
            int errno_;
            char *path;
        }libc_path;

        struct {
            char *source_path;
            char *func;
            int lineno;
        } invalid_argument;
    }u;
};

void ATR_error_clear(struct ATR *atr, struct ATR_Error *e);
char *ATR_strerror(struct ATR *atr, struct ATR_Error *e);

void ATR_set_libc_path_error(struct ATR *atr,
                             struct ATR_Error *e,
                             int errno_,
                             const char *path);

void ATR_set_invalid_argument(struct ATR *atr,
                              struct ATR_Error *e,
                              const char *source_path,
                              const char *func,
                              int lineno);


#ifdef __cplusplus
}
#endif

#endif