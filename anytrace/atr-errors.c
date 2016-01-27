#include <stdlib.h>
#include <string.h>

#include "atr-errors.h"
#include "npr/strbuf.h"

void
ATR_error_clear(struct ATR *atr, struct ATR_Error *e)
{
    switch(e->code) {
    case ATR_NO_ERROR:
    case ATR_LIBC_ERROR:
        break;

    case ATR_LIBC_PATH_ERROR:
        free(e->u.libc_path.path);
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

    case ATR_INVALID_ARGUMENT:
        npr_strbuf_printf(&sb,
                          "%s:%s:%d:invalid argument",
                          e->u.invalid_argument.source_path,
                          e->u.invalid_argument.func,
                          e->u.invalid_argument.lineno,
                          strerror(e->u.libc.errno_));
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
