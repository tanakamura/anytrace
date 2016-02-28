#include <stdlib.h>
#include "anytrace/atr.h"
#include "anytrace/atr-language-module.h"

void
ATR_init(struct ATR *atr)
{
    npr_symbol_init();
    atr->last_error.code = ATR_NO_ERROR;

    atr->cap_language = 1;
    atr->num_language = 0;
    atr->languages = malloc(sizeof(struct ATR_language_module) * 1);

    ATR_load_language_module(atr);
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
    free(atr->languages);
}
