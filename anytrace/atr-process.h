#ifndef ANYTRACE_PROCESS_H
#define ANYTRACE_PROCESS_H

#include <stdint.h>
#include <stdio.h>

#include "npr/int-map.h"
#include "npr/symbol.h"
#include "npr/mempool.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ATR;

struct ATR_module {
    struct npr_symbol *path;
};

struct ATR_mapping {
    uintptr_t start, end, offset;

    int module;
};

struct ATR_process {
    struct npr_mempool allocator;
    int pid;

    int num_module;
    struct ATR_module *modules;

    int num_mapping;
    struct ATR_mapping *mappings;

    int num_task;
    int *tasks;
};


/* return negative if failed 
 * (process will be suspended if succeeded)
 */
int ATR_open_process(struct ATR_process *dst,
                     struct ATR *atr,
                     int pid);

void ATR_close_process(struct ATR *atr,
                       struct ATR_process *proc);

void ATR_dump_process(FILE *fp,
                      struct ATR *atr,
                      struct ATR_process *proc);

struct ATR_map_info {
    struct npr_symbol *path;
    uintptr_t offset;
};

/* return negative if failed */
int ATR_lookup_map_info(struct ATR_map_info *info,
                        struct ATR *atr,
                        struct ATR_process *proc,
                        uintptr_t addr);

#ifdef __cplusplus
}
#endif

#endif
