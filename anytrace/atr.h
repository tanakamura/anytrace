#ifndef ATR_H
#define ATR_H

#include "anytrace/atr-errors.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ATR {
    struct ATR_Error last_error;
};

void ATR_init(struct ATR *atr);
void ATR_fini(struct ATR *atr);

void ATR_free(struct ATR *atr, void *p);

void ATR_perror(struct ATR *atr);

#ifdef __cplusplus
}
#endif

#endif