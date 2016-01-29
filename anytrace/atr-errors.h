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
    ATR_YAMA_ENABLED,
    ATR_MAP_NOT_FOUND,
    ATR_UNKNOWN_MAPPED_FILE_TYPE,

    ATR_FRAME_INFO_NOT_FOUND,

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

        struct {
            char *path;
        } unknown_file_type;

        struct {
            uintptr_t addr;
        } map_not_found;

        struct {
            struct npr_symbol *path;
            uintptr_t offset;
        } frame_info_not_found;
    }u;
};

void ATR_error_clear(struct ATR *atr, struct ATR_Error *e);
char *ATR_strerror(struct ATR *atr, struct ATR_Error *e);

void ATR_set_error_code(struct ATR *atr,
                        struct ATR_Error *e,
                        enum ATR_error_code c);
void ATR_set_libc_path_error(struct ATR *atr,
                             struct ATR_Error *e,
                             int errno_,
                             const char *path);

void ATR_set_invalid_argument(struct ATR *atr,
                              struct ATR_Error *e,
                              const char *source_path,
                              const char *func,
                              int lineno);

void ATR_set_unknown_mapped_file_type(struct ATR *atr,
                                      struct ATR_Error *e,
                                      const char *path);

void ATR_set_frame_info_not_found(struct ATR *atr,
                                  struct ATR_Error *e,
                                  struct npr_symbol *path,
                                  uintptr_t offset);

#ifdef __cplusplus
}
#endif

#endif