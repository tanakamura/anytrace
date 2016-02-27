#ifndef ATR_BACKTRACE_H
#define ATR_BACKTRACE_H

#include <stdint.h>
#include "anytrace/atr-file.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ATR_TRACER_NUM_REG 18

#define X8664_CFA_REG_RIP 16
#define X8664_CFA_REG_RSP 7

enum ATR_backtracer_state {
    ATR_BACKTRACER_OK,

    ATR_BACKTRACER_HAVE_ERROR,
    ATR_BACKTRACER_FRAME_BOTTOM,
};

struct ATR_backtracer {
    enum ATR_backtracer_state state;

    struct ATR_file current_module;
    uintptr_t pc_offset_in_module;
    uint64_t cfa_regs[ATR_TRACER_NUM_REG];      // stored in dwarf order
};

struct ATR_process;
struct ATR_file;
struct ATR;

int ATR_backtrace_init(struct ATR *atr,
                       struct ATR_backtracer *tr,
                       struct ATR_process *proc);

/* return -1 if failed */
int ATR_backtrace_up(struct ATR *atr,
                     struct ATR_backtracer *tr,
                     struct ATR_process *proc);

void ATR_backtrace_fini(struct ATR *atr, struct ATR_backtracer *tr);


#ifdef __cplusplus
}
#endif

#endif