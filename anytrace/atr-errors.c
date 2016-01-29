#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "atr-errors.h"
#include "npr/strbuf.h"

void
ATR_error_clear(struct ATR *atr, struct ATR_Error *e)
{
    switch(e->code) {
    case ATR_NO_ERROR:
    case ATR_LIBC_ERROR:
    case ATR_YAMA_ENABLED:
    case ATR_MAP_NOT_FOUND:
    case ATR_FRAME_INFO_NOT_FOUND:
        break;

    case ATR_LIBC_PATH_ERROR:
        free(e->u.libc_path.path);
        break;

    case ATR_UNKNOWN_MAPPED_FILE_TYPE:
        free(e->u.unknown_file_type.path);
        break;

    case ATR_INVALID_ARGUMENT:
        free(e->u.invalid_argument.source_path);
        free(e->u.invalid_argument.func);
        break;
    }

    e->code = ATR_NO_ERROR;
}
char *
ATR_strerror(struct ATR *atr, struct ATR_Error *e)
{
    struct npr_strbuf sb;

    npr_strbuf_init(&sb);

    switch (e->code) {
    case ATR_NO_ERROR:
        npr_strbuf_puts(&sb, "no error");
        break;

    case ATR_LIBC_ERROR:
        npr_strbuf_puts(&sb, strerror(e->u.libc.errno_));
        break;


    case ATR_LIBC_PATH_ERROR:
        npr_strbuf_printf(&sb,
                          "%s:%s",
                          e->u.libc_path.path,
                          strerror(e->u.libc.errno_));
        break;

    case ATR_YAMA_ENABLED:
        npr_strbuf_printf(&sb,
                          "yama is enabled. you should set /proc/sys/kernel/yama to '0'");
        break;

    case ATR_INVALID_ARGUMENT:
        npr_strbuf_printf(&sb,
                          "%s:%s:%d:invalid argument",
                          e->u.invalid_argument.source_path,
                          e->u.invalid_argument.func,
                          e->u.invalid_argument.lineno,
                          strerror(e->u.libc.errno_));
        break;

    case ATR_UNKNOWN_MAPPED_FILE_TYPE:
        npr_strbuf_printf(&sb,
                          "'%s' is unknown file",
                          e->u.unknown_file_type.path);
        break;

    case ATR_MAP_NOT_FOUND:
        npr_strbuf_printf(&sb,
                          "map not found(addr=%016"PRIxPTR")",
                          e->u.map_not_found.addr);
        break;

    case ATR_FRAME_INFO_NOT_FOUND:
        npr_strbuf_printf(&sb,
                          "frame info not found(path=%s, offset=%016"PRIxPTR")",
                          e->u.frame_info_not_found.path->symstr,
                          e->u.frame_info_not_found.offset);
        break;
    }

    char *ret = npr_strbuf_strdup(&sb);
    npr_strbuf_fini(&sb);

    return ret;
}

void
ATR_set_libc_path_error(struct ATR *atr,
                        struct ATR_Error *e,
                        int errno_,
                        const char *path)
{
    ATR_error_clear(atr, e);

    e->code = ATR_LIBC_PATH_ERROR;
    e->u.libc_path.errno_ = errno_;
    e->u.libc_path.path = strdup(path);
}

void
ATR_set_invalid_argument(struct ATR *atr,
                         struct ATR_Error *e,
                         const char *source_path,
                         const char *func,
                         int lineno)
{
    ATR_error_clear(atr, e);

    e->code = ATR_INVALID_ARGUMENT;
    e->u.invalid_argument.source_path = strdup(source_path);
    e->u.invalid_argument.func = strdup(func);
    e->u.invalid_argument.lineno = lineno;
}

void
ATR_set_unknown_mapped_file_type(struct ATR *atr,
                                 struct ATR_Error *e,
                                 const char *path)
{
    ATR_error_clear(atr, e);

    e->code = ATR_UNKNOWN_MAPPED_FILE_TYPE;
    e->u.unknown_file_type.path = strdup(path);
}


void
ATR_set_error_code(struct ATR *atr,
                   struct ATR_Error *e,
                   enum ATR_error_code c)
{
    ATR_error_clear(atr, e);

    e->code = c;
}

void
ATR_set_frame_info_not_found(struct ATR *atr,
                             struct ATR_Error *e,
                             struct npr_symbol *path,
                             uintptr_t offset)
{
    ATR_error_clear(atr, e);

    e->code = ATR_FRAME_INFO_NOT_FOUND;
    e->u.frame_info_not_found.path = path;
    e->u.frame_info_not_found.offset = offset;

}