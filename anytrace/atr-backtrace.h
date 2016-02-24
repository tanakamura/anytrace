#ifndef ATR_BACKTRACE_H
#define ATR_BACKTRACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ATR_TRACER_NUM_REG 18

#define X8664_CFA_REG_RIP 16
#define X8664_CFA_REG_RSP 7

struct ATR_backtracer {
    uint64_t cfa_regs[ATR_TRACER_NUM_REG];      // stored in dwarf order
};

struct ATR_process;
struct ATR_file;
struct ATR;

void ATR_backtrace_init(struct ATR_backtracer *tr,
                        struct ATR_process *proc);

/* return -1 if failed */
int ATR_backtrace_up(struct ATR *atr,
                     struct ATR_backtracer *tr,
                     struct ATR_process *proc);

void ATR_backtrace_fini(struct ATR_backtracer *tr);


#ifdef __cplusplus
}
#endif

#endif