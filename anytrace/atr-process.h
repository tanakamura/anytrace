#ifndef ANYTRACE_PROCESS_H
#define ANYTRACE_PROCESS_H

#include <stdint.h>

#include "npr/int-map.h"
#include "npr/mempool.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ATR_Module {
    struct ATR_Process *proc;

    uintptr_t start, end;
    const char *path;
};

struct ATR_Process {
    struct npr_mempool allocator;
    int pid;

    struct npr_symtab module_table;
    int ptrace_fd;

    int num_module;
    struct ATR_Module *modules;
};


/* return negative if failed 
 * (process will be suspended if succeeded)
 */
int ATR_open_process(struct ATR_Process *dst,
                     struct ATR *atr,
                     int pid);

void ATR_close_process(struct ATR *atr,
                       struct ATR_Process *proc);

#ifdef __cplusplus
}
#endif

#endif
