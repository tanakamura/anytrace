#include <stdlib.h>
#include "atr.h"

void
ATR_init(struct ATR *atr)
{
    atr->last_error.code = ATR_NO_ERROR;
}

void
ATR_free(struct ATR *st, void *p)
{
    free(p);
}

void
ATR_perror(struct ATR *atr)
{
    char *p = ATR_strerror(atr, &atr->last_error);
    fputs(p, stderr);
    fputc('\n', stderr);
    free(p);
}

void
ATR_fini(struct ATR *atr)
{
    ATR_error_clear(atr, &atr->last_error);
}
